from ConfigSpace import Categorical, Float, UniformFloatHyperparameter, Configuration, ConfigurationSpace, InCondition
from smac import AlgorithmConfigurationFacade, Scenario

from lab.calls.call import Call
from gnn_training import ModelSetting, PreprocessorSettings, run_step_gnn_learning

import sys
import os
import subprocess
import re
import shutil
import math


# from functools import partial

# Hardcoded paths that depend on the trraining part. This could be passed by parameter instead
GNN_REPO_DIR = 'gnn-learning'
SCORPION_DIR = 'scorpion'


# Hardcoded paths
INTERMEDIATE_SMAC_MODELS = 'intermediate-smac-models'


class Eval:
    def __init__(self, ROOT, DATA_DIR, WORKING_DIR, domain_file, instances_dir, instances_properties, extra_flags):
        self.DATA_DIR = DATA_DIR
        self.MY_DIR = os.path.dirname(os.path.realpath(__file__))
        self.GNN_REPO_DIR = os.path.join(os.path.join(ROOT, "gnn-learning"))
        self.GNN_DATA_DIR = os.path.join(self.DATA_DIR, "gnn-data")
        self.GNN_LEARNING_DIR = os.path.join(self.DATA_DIR, "gnn-learning")
        self.extra_flags = extra_flags

        self.SCORPION_PATH = os.path.join(ROOT, "scorpion")
        # self.candidate_models=candidate_models

        self.SMAC_MODELS_DIR = os.path.abspath(os.path.join(WORKING_DIR, INTERMEDIATE_SMAC_MODELS))
        if os.path.exists(self.SMAC_MODELS_DIR):
            shutil.rmtree(self.SMAC_MODELS_DIR)
        os.mkdir(self.SMAC_MODELS_DIR)
        self.instances_dir = instances_dir
        self.instances_properties = instances_properties
        self.domain_file = domain_file

        self.regex_total_time = re.compile(rb"INFO\s+Planner time:\s(.+)s", re.MULTILINE)
        self.regex_operators = re.compile(r"([\d]+) of [\d]+ operators necessary")
        self.is_retries_excited = re.compile(r"Retries exceeded")
        self.regex_plan_cost = re.compile(rb"\[t=.*s, .* KB\] Plan cost:\s(.+)\n", re.MULTILINE)
        self.regex_no_solution = re.compile(rb"\[t=.*KB\] Completely explored state space.*no solution.*", re.MULTILINE)

    def target_function(self, config: Configuration, instance: str, seed: int) -> float:
        model_settings, target_folder = parse_config(config)

        DOMAIN = f'{self.instances_dir}/{self.domain_file}'
        PROBLEM = f'{self.instances_dir}/{instance}.pddl'

        print(f"Domain: {DOMAIN}")
        print(f"Problem: {PROBLEM}")
        print(f"Running {instance} with {model_settings} and {target_folder}")

        # TODO: setup limits
        model_path = run_step_gnn_learning(self.GNN_REPO_DIR, model_settings, f'{self.GNN_DATA_DIR}/{target_folder}', f'{self.GNN_LEARNING_DIR}/{target_folder}', time_limit=1200, memory_limit=4*1024*1024)

        if model_path is None:
            return math.inf
        
        preprocessor_setting = PreprocessorSettings(
            model_path=model_path,
            gnn_retries=0,
            gnn_threshold=0.5
        ).to_parameter_string()

        command = [sys.executable, f'{self.SCORPION_PATH}/fast-downward.py']\
                + self.extra_flags\
                + [
                    '--alias', 'lama-first', 
                    '--transform-task-options', preprocessor_setting,
                    '--transform-task', f'{self.GNN_REPO_DIR}/src/preprocessor.command',
                    '--overall-time-limit', '2400',
                    '--search-time-limit', '180',
                    DOMAIN,
                    PROBLEM]
        proc = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)


        try:
            output, error_output = proc.communicate(timeout=60*10) # Timeout in seconds TODO: set externally
            read_output = output.decode()

            total_time = self.regex_total_time.search(output)
            all_matches_num_operators = self.regex_operators.findall(read_output)
            plan_cost = self.regex_plan_cost.search(output)
            is_retries_excited = self.is_retries_excited.search(read_output)

            if is_retries_excited:
                print (f"Ran {instance} with model settings {model_settings}: not solved due to retries exceeded")
                return 10000000

            if total_time and all_matches_num_operators and plan_cost:
                total_time = float(total_time.group(1))
                num_operators = float(all_matches_num_operators[-1])
                plan_cost = float(plan_cost.group(1))
                print (f"Ran {instance} with model settings {model_settings}: time {total_time}, operators {num_operators}, cost {plan_cost}")
                return total_time
            elif self.regex_no_solution.search(output):
                print (f"Ran {instance} with model settings {model_settings}: not solved due to partial grounding")
                #print(output.decode())
                return 10000000
            else:
                print (f"WARNING: Ran {instance} with model settings {model_settings}: not solved due to unknown reasons")

                print("Output: ", output.decode())
                if error_output:
                    print("Error Output: ", error_output.decode())
                return 10000000
        except subprocess.CalledProcessError:
            print (f"WARNING: Command failed: {' '.join(command)}")
            print (f"Ran {instance} with model settings {model_settings}: not solved due to crash")
            return 10000000

        except subprocess.TimeoutExpired:
            proc.kill()
            print (f"RRan {instance} with model settings {model_settings}: not solved due to time limit")
            return 10000000


        except Exception as e:
            print (f"Error: Command failed: {' '.join(command)}")

            print("Output: ", output.decode())
            if error_output:
                print("Error Output: ", error_output.decode())


def parse_config(config):
    config_dict = config.get_dictionary()

    modelSettingsDict = {
        'aggr': config_dict['aggr'],
        'conv_type': config_dict['conv_type'],
        'hidden_size': config_dict['hidden_size'],
        'layers_num': config_dict['layers_num'],
        'lr': config_dict['lr'],
        'optimizer': config_dict['optimizer'],
    }
    target_operators = config_dict['target_folder']

    model_settings = ModelSetting.from_dict(modelSettingsDict)

    return model_settings, target_operators

# Note: default configuration should solve at least 50% of the instances. Pick instances
# with LAMA accordingly. If we run SMAC multiple times, we can use different instances
# set, as well as changing the default configuration each time.
def run_smac(ROOT, DATA_DIR, WORKING_DIR, domain_file, instance_dir, instances_with_features : dict, instances_properties : dict, walltime_limit, n_trials, n_workers, run_id, extra_flags):
    GNN_LEARNING_DIR = os.path.join(DATA_DIR, 'gnn-learning')
    DATA_DIR = os.path.abspath(DATA_DIR) # Make sure path is absolute so that symlinks work
    working_dir = f'{WORKING_DIR}'+f'-run-{run_id}'
    os.mkdir(working_dir)

    ############################
    ### Create model parameters
    #############################

    target_folder = Categorical('target_folder', ['mixed', "runs-lama", "good-operators-unit"], default="good-operators-unit")
    layers_num = Categorical('layers_num', [3,4,5,6], default=4)
    hidden_size = Categorical('hidden_size', [4,8,16,], default=8)
    conv_type = Categorical('conv_type', ['SAGEConv'], default='SAGEConv')
    aggr = Categorical('aggr', ['mean'], default='mean')
    optimizer = Categorical('optimizer', ['Adam'], default='Adam')
    lr = UniformFloatHyperparameter('lr', 1e-03, 0.02)

    parameters = [target_folder, layers_num, hidden_size, conv_type, aggr, optimizer, lr]
 



    cs = ConfigurationSpace(seed=2023) # Fix seed for reproducibility
    cs.add_hyperparameters(parameters)
    # cs.add_conditions(conditions)

    evaluator = Eval (ROOT, DATA_DIR, working_dir, domain_file, instance_dir, instances_properties, extra_flags=extra_flags)


    print ([ins for ins in instances_with_features])
    print(instances_with_features)
    scenario = Scenario(
        configspace=cs, deterministic=True,
        output_directory=os.path.join(working_dir, 'smac'),
        walltime_limit=walltime_limit,
        n_trials=n_trials,
        n_workers=n_workers,
        instances=[ins for ins in instances_with_features],
        instance_features=instances_with_features,
        # objectives=["cost", "time", "operators"]
    )
    # Use SMAC to find the best configuration/hyperparameters
    smac = AlgorithmConfigurationFacade(scenario, evaluator.target_function, overwrite=False)
    incumbent = smac.optimize()

    print("Best configuration: ", incumbent)    
    model_setting, target_folder = parse_config(incumbent)

    path_to_best_model = os.path.join(GNN_LEARNING_DIR, target_folder, 'models', model_setting.dir_name, '0.pt')
    
    print("Chosen model settings: ", model_setting)
    return path_to_best_model, model_setting


def check_model_score_on_problem(path_to_model_dir, domain_path, PROBLEM, extra_flags):
    regex_operators = re.compile(r"([\d]+) of [\d]+ operators necessary")

    preprocessr_settings_file = os.path.join(path_to_model_dir, 'preprocessor_settings.txt')
    with open(preprocessr_settings_file, 'r') as f:
        preprocess_settings = f.read()

    command = [sys.executable, f'{SCORPION_DIR}/fast-downward.py'] +  extra_flags + ['--alias', 'lama-first', '--transform-task-options', preprocess_settings, '--transform-task', f'{GNN_REPO_DIR}/src/preprocessor.command', domain_path, PROBLEM]
    proc = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    output, error_output = proc.communicate()

    output = output.decode()

    operators = regex_operators.findall(output)

    result = float(operators[-1])
    return result


def compare_models(path_to_best, path_to_candidate, domain_path, instances_dir, instances, extra_flags) -> bool:
    comaprison_results = []

    for problem in instances:
        PROBLEM = f'{instances_dir}/{problem}.pddl'

        best_operators = check_model_score_on_problem(path_to_best, domain_path, PROBLEM, extra_flags)
        candidate_operators = check_model_score_on_problem(path_to_candidate, domain_path, PROBLEM, extra_flags)

        if best_operators < candidate_operators:
            comaprison_results.append(1)
        elif best_operators > candidate_operators:
            comaprison_results.append(0)
        else:
            comaprison_results.append(-1)

    if comaprison_results.count(1) >= comaprison_results.count(0):
        return False
    else:
        return True
