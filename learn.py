#! /usr/bin/env python

from __future__ import print_function

import argparse
import os
import os.path
import shutil
import sys
import tarfile

from lab.calls.call import Call
from lab.environments import  LocalEnvironment

debug=False

NONE_FLAG = []
extra_flags = NONE_FLAG

sys.path.append(f'{os.path.dirname(__file__)}/training')
import training
if debug:
    from training.gnn_training import run_step_gnn_learning, PreprocessorSettings, ModelSetting
    from training.generate_gnn_data import run_step_generate_gnn_data
    from training.run_experiment import RunExperiment
    from training.utils import (
        select_instances_with_properties,
        save_model,
    )
    from training.instance_set import InstanceSet, select_instances_from_runs
    from training.optimize_smac import run_smac, compare_models

else:
    from gnn_training import run_step_gnn_learning, ModelSetting, PreprocessorSettings
    from generate_gnn_data import run_step_generate_gnn_data
    from run_experiment import RunExperiment
    from utils import (
        save_model,
    )
    from instance_set import InstanceSet, select_instances_from_runs
    from optimize_smac import run_smac, compare_models




from downward import suites

# All time limits are in seconds
TIME_LIMITS_IPC_SINGLE_CORE = {
    'run_experiment' : 15*60, # 10 minutes
    'smac-optimization' : 60*60*7, # 1 hour
}

TIME_LIMITS_SEC = TIME_LIMITS_IPC_SINGLE_CORE

# Memory limits are in MB
MEMORY_LIMITS_MB = {
    'run_experiment' : 1024*16,
}


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("domain", help="path to domain file")
    parser.add_argument("problem", nargs="+", help="path to problem file")
    parser.add_argument("--domain_knowledge_file", help="path to store knowledge file.")
    parser.add_argument("--path", default='data', help="path to store results")
    parser.add_argument("--cpus", type=int, default=1, help="number of cpus available")
    parser.add_argument("--total_time_limit", default=30, type=int, help="time limit")
    parser.add_argument("--total_memory_limit", default=7*1024, help="memory limit")

    return parser.parse_args()


def main():
    args = parse_args()

    ROOT = os.path.dirname(os.path.abspath(__file__))

    TRAINING_DIR=args.path

    REPO_GOOD_OPERATORS = f"{ROOT}/fd-symbolic"
    REPO_LEARNING = f"{ROOT}/learning"
    BENCHMARKS_DIR = f"{TRAINING_DIR}/instances"
    INSTANCES_SMAC = f"{TRAINING_DIR}/instances-smac"

    REPO_GNN_LEARNING = f"{ROOT}/gnn-learning"
    GNN_DATA_DIR = f"{TRAINING_DIR}/gnn-data"
    GNN_LEARNING_DIR = f"{TRAINING_DIR}/gnn-learning"

    DK_OUTPUT=args.domain_knowledge_file

    if os.path.exists(TRAINING_DIR):
        shutil.rmtree(TRAINING_DIR)
    os.mkdir(TRAINING_DIR)

    # Copy all input benchmarks to the directory
    if os.path.isdir(args.domain): # If the first argument is a folder instead of a domain file
        shutil.copytree(args.domain, BENCHMARKS_DIR)
        args.domain += "/domain.pddl"
    else:
        os.mkdir(BENCHMARKS_DIR)
        shutil.copy(args.domain, BENCHMARKS_DIR)
        for problem in args.problem:
            shutil.copy(problem, BENCHMARKS_DIR)

    
    ENV = LocalEnvironment(processes=args.cpus)
    SUITE_ALL = suites.build_suite(TRAINING_DIR, ['instances'])

    
    RUN = RunExperiment (TIME_LIMITS_SEC ['run_experiment'], MEMORY_LIMITS_MB['run_experiment'])

    RUN.run_planner(f'{TRAINING_DIR}/runs-lama', REPO_GOOD_OPERATORS, [], ENV, SUITE_ALL, driver_options = ["--alias", "lama-first"])

    instances_manager = InstanceSet(f'{TRAINING_DIR}/runs-lama')

    # We run the good operators tool only on instances solved by lama in less than 30 seconds
    instances_to_run_good_operators = instances_manager.select_instances([lambda i, p : p['search_time'] < 30])

    SUITE_GOOD_OPERATORS = suites.build_suite(TRAINING_DIR, [f'instances:{name}.pddl' for name in instances_to_run_good_operators])
    RUN.run_good_operators(f'{TRAINING_DIR}/good-operators-unit', REPO_GOOD_OPERATORS, ['--search', "sbd(store_operators_in_optimal_plan=true, cost_type=1)"], ENV, SUITE_GOOD_OPERATORS)
    instances_manager.add_training_data(f'{TRAINING_DIR}/good-operators-unit')

    # has_action_cost = len(select_instances_from_runs(f'{TRAINING_DIR}/good-operators-unit', lambda p : p['use_metric'])) > 0
    # if has_action_cost:
    #     RUN.run_good_operators(f'{TRAINING_DIR}/good-operators-cost', REPO_GOOD_OPERATORS, ['--search', "sbd(store_operators_in_optimal_plan=true)"], ENV, SUITE_GOOD_OPERATORS)
    #     instances_manager.add_training_data(f'{TRAINING_DIR}/good-operators-cost')

    TRAINING_INSTANCES = instances_manager.split_training_instances()

    
    # TODO
    data_folders = []
    good_operators_data = f'{TRAINING_DIR}/good-operators-unit'
    gnn_data_good_ops = f'{GNN_DATA_DIR}/good-operators-unit'
    gnn_model_data_good_ops = f'{GNN_LEARNING_DIR}/good-operators-unit'

    lama_operators_data = f'{TRAINING_DIR}/runs-lama'
    gnn_data_lama_ops = f'{GNN_DATA_DIR}/runs-lama'
    gnn_model_data_lama_ops = f'{GNN_LEARNING_DIR}/runs-lama'

    mixed_operators_data = f'{TRAINING_DIR}/mixed'
    gnn_data_mixed = f'{GNN_DATA_DIR}/mixed'
    gnn_model_data_mixed =  f'{GNN_LEARNING_DIR}/mixed'

    shutil.copytree(lama_operators_data, mixed_operators_data)


    # TODO: Currently the strategy is to take the good_operators file as a priority and if missing we take the plan from lama
    # This migght also need to support multiple plans concatanation
    # First copy all LAMA plans to the mixed folder
    # Then replace the plans with good_operators if LAMA and GOOD overlap
    problems_in_both = set(os.listdir(good_operators_data)).intersection(set(os.listdir(lama_operators_data)))
    for problem_dir in problems_in_both:
        good_problem_dir = f'{good_operators_data}/{problem_dir}'
        # double check we have good_operators
        if os.path.exists(f'{good_problem_dir}/good_operators'):
            shutil.copy(f'{good_problem_dir}/good_operators', f'{mixed_operators_data}/{problem_dir}')
            print("Copied good operators from good_operators_unit")


    data_folders.append((good_operators_data, gnn_data_good_ops))
    data_folders.append((lama_operators_data, gnn_data_lama_ops))
    data_folders.append((mixed_operators_data, gnn_data_mixed))




    for problems_dir, output_dir in data_folders:
        run_step_generate_gnn_data(
            REPO_GNN_LEARNING=REPO_GNN_LEARNING,
            PROBLEMS_DIR=problems_dir,
            OUTPUT_DIR=output_dir,
            extra_flags=extra_flags,  # NONE FLAG IS SET
            time_limit=600, # TODO: what should it be 
            memory_limit=4*1024*1024 # TODO: what should it be
        )


    DK_DIR = os.path.join(TRAINING_DIR, "DK")
    if os.path.exists(DK_DIR):
        shutil.rmtree(DK_DIR)
    os.mkdir(DK_DIR)

    TEMP_DIR = os.path.join(TRAINING_DIR, "intermediate")

    SMAC_INSTANCES = instances_manager.get_smac_instances(['translator_operators', 'translator_facts', 'translator_variables'])

    for i in range(3):
        model_path, model_setting = run_smac(
            ROOT, f'{TRAINING_DIR}', f'{TRAINING_DIR}/smac', args.domain, BENCHMARKS_DIR, SMAC_INSTANCES,
            instances_manager.get_instance_properties(), walltime_limit=TIME_LIMITS_SEC['smac-optimization'],
            n_trials=300, n_workers=1, run_id=i, extra_flags=extra_flags)
        #model_path, model_setting = run_smac( ROOT, f'{TRAINING_DIR}', f'{TRAINING_DIR}/smac', args.domain, BENCHMARKS_DIR, SMAC_INSTANCES, instances_manager.get_instance_properties(), walltime_limit=100, n_trials=100, n_workers=1, run_id=i)

        if i == 0:
            # Copy the best model to the DK folder
            save_model(model_path, DK_DIR, model_setting, extra_flags)
            make_tarfile(DK_DIR, f'{DK_OUTPUT}.{i}')
        else:
            if os.path.exists(TEMP_DIR):
                shutil.rmtree(TEMP_DIR)
            os.mkdir(TEMP_DIR)

            save_model(model_path, TEMP_DIR, model_setting, extra_flags)

            is_candidate_better = compare_models(DK_DIR, TEMP_DIR, args.domain, BENCHMARKS_DIR, SMAC_INSTANCES, extra_flags=extra_flags)
            print(f"Is candidate better: {is_candidate_better}")
            # input("Press Enter to continue...")

            if is_candidate_better:
                preprocessor_settings = PreprocessorSettings.from_file(f'{TEMP_DIR}/preprocessor_settings.txt')
                preprocessor_settings.model_path = f'{DK_DIR}/model.pt'
                preprocessor_settings_txt = preprocessor_settings.to_parameter_string()
                # remove preprocessor_settings.txt from TEMP_DIR
                os.remove(f'{TEMP_DIR}/preprocessor_settings.txt')
                # save preprocessor_settings_txt to TEMP_DIR
                with open(f'{TEMP_DIR}/preprocessor_settings.txt', 'w') as f:
                    f.write(preprocessor_settings_txt)
                # remove DK_DIR
                shutil.rmtree(DK_DIR)
                # copy TEMP_DIR to DK_DIR
                shutil.copytree(TEMP_DIR, DK_DIR)
                # make tarfile
                make_tarfile(DK_DIR, f'{DK_OUTPUT}.{i}')




def save_model(source_model, target_dir, model_setting, extra_flags):

    shutil.copy(source_model, f'{target_dir}/model.pt')

    # save extra flags
    with open(os.path.join(target_dir, "extra_flags.txt"), "w") as f:
        f.write(str(extra_flags))

    with open(os.path.join(target_dir, "model_settings.txt"), "w") as f:
        f.write(model_setting.to_parameter_string_comma())

    preprocessor_settings = PreprocessorSettings(gnn_retries=3, gnn_threshold=0.5, model_path=f'{target_dir}/model.pt')
    with open(os.path.join(target_dir, "preprocessor_settings.txt"), "w") as f:
        f.write(preprocessor_settings.to_parameter_string())

def make_tarfile(source_dir, output_filename):
        #Update preprocess settings
        preprocessor_settings = PreprocessorSettings.from_file(f'{source_dir}/preprocessor_settings.txt')
        model_folder = source_dir.split('/')[-1]
        preprocessor_settings.model_path = f'extracted/{model_folder}/model.pt'
        preprocessor_settings_txt = preprocessor_settings.to_parameter_string()
        # remove preprocessor_settings.txt from source_dir
        os.remove(f'{source_dir}/preprocessor_settings.txt')
        # save preprocessor_settings_txt to source_dir
        with open(f'{source_dir}/preprocessor_settings.txt', 'w') as f:
            f.write(preprocessor_settings_txt)
        
        with tarfile.open(output_filename, "w:gz") as tar:
            tar.add(source_dir, arcname=os.path.basename(source_dir))
    

if __name__ == "__main__":
    main()
