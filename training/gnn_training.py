import os
import sys
import shutil
import subprocess
from lab.calls.call import Call
from dataclasses import dataclass, field

CONVOLUTIONS = {
    "SAGEConv",
    "GATConv",
    "GCNConv"
}

AGGREGATIONS = {
    "mean",
    "max",
    "min",
    "sum",
    "var",
    "median",
    "std",
    "add",
}

OPTIMIZER_CLASSES = {
    "Adam",
    "RMSprop",
    "Adagrad"
}

def run_step_gnn_learning(REPO_LEARNING, model_setting:"ModelSetting", problems_dir, output_dir, time_limit=1200, memory_limit = 4*1024*1024):
    train_dir = os.path.join(problems_dir, "train")
    test_dir = os.path.join(problems_dir, "test")


    if os.path.exists(train_dir):
        shutil.rmtree(train_dir)
    if os.path.exists(test_dir):
        shutil.rmtree(test_dir)

    problems = os.listdir(problems_dir)
    number_of_problems = len(problems)
    
    os.mkdir(train_dir)
    os.mkdir(test_dir)


    # TODO TEMPORARY
    def split_instances(problems_dir, problems, train_dir, test_dir):
        for problem in problems:
            if problem == "train" or problem == "test":
                continue
            original_problem_dir = os.path.join(problems_dir, problem)
            dst = os.path.join(train_dir, problem)
            shutil.copytree(original_problem_dir, dst)
            

    split_instances(problems_dir, problems, train_dir, test_dir)
    assert len(os.listdir(train_dir)) == len(problems)
    assert len(os.listdir(train_dir)) == number_of_problems

    train_size = len(os.listdir(train_dir))
    test_size = len(os.listdir(test_dir))

    # TODO REMOVE THAT
    assert test_size == 0
    assert train_size + test_size == number_of_problems

    setting = model_setting.to_parameter_string()
    # command = [sys.executable, f'{REPO_LEARNING}/src/train.py', train_dir, test_dir, output_dir, "--model-settings", setting]
    # proc = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    # output, output_error = proc.communicate(timeout=300) #TODO: parametrize

    # print("Output: " + str(output))
    # print("Output error: " + str(output_error))
    # 5/0

    # return output, output_error

    num_epoch = str(1000)


    Call([sys.executable, f'{REPO_LEARNING}/src/train.py', train_dir, output_dir, "--model-settings", setting, "--num-epoch", num_epoch], "train-gnn", time_limit=time_limit, memory_limit=memory_limit).wait()

    # TODO: We currently have only one model, but in future we will have multiple models
    # latest_model = None if len(os.listdir(f'{output_dir}/models/{model_settings.dir_name}')) == 0 else len(os.listdir(f'{output_dir}/models/{model_settings.dir_name}')) - 1
    # new_model_path = f'{output_dir}/models/{model_settings.dir_name}/{latest_model}.pt'
    # if latest_model is None:
    #     return None
    # return new_model_path

    # Assert the file exisits
    model_path = f'{output_dir}/models/{model_setting.dir_name}/0.pt'
    assert os.path.exists(model_path), f"Model path {model_path} does not exist"
    if not os.path.exists(model_path):
        return None
    return model_path


    # Make domain knowledge folder
    # DK_DIR = os.path.join(DATA_DIR, "DK")
    # if os.path.exists(DK_DIR):
    #     shutil.rmtree(DK_DIR)
    # os.mkdir(DK_DIR)


    # Copy the best model to the DK folder
    # best_model_path = choose_best_model(output_dir, default=True)
    # shutil.copy(best_model_path, os.path.join(DK_DIR, "model.pt"))

    # # TODO: Save model settings as string in the DK folder
    # with open(os.path.join(DK_DIR, "model_settings.txt"), "w") as f:
    #     f.write(adam.to_parameter_string_comma())

    # # TODO: Save the preprocessor settings as string to the DK folder
    # preprocessor_settings = PreprocessorSettings(gnn_retries=3, gnn_threshold=0.5)
    # with open(os.path.join(DK_DIR, "preprocessor_settings.txt"), "w") as f:
    #     f.write(preprocessor_settings.to_parameter_string())
        
    # DK_DIR into zip file
    # shutil.make_archive(DK_DIR, 'zip', DK_DIR)


# def choose_best_model(output_dir, default=True):
#     if default:
#         return os.path.join(output_dir, "models", "4-64-SAGEConv-mean-Adam-0.001", "0.pt")
    
#     # For each of our good_actions recognision strategies get the dirs
#     # Example:
#         # - good-operators-unit
#         # - lama
#         # - mixed

#     # Setup the best model path and score
#     best_model_path, best_model_score = None, math.inf

#     # Get all folders under output_dir/models
#     model_actions_strategies_dir = os.listdir(output_dir)

#     # for each [good-operators-unit, lama, mixed]
#     for recognision_strategy in model_actions_strategies_dir:
#         # Get all architectures under the recognision strategy
#         model_architecture_dirs = os.listdir(os.path.join(output_dir, recognision_strategy))
#         for model_architecture_dir in model_architecture_dirs:
#             actual_path = os.path.join(output_dir, recognision_strategy, model_architecture_dir)
#             best_model_for_architecture_path, architecture_best_score = get_best_model_for_architecture(actual_path)
                
#             if architecture_best_score < best_model_score:
#                 best_model_score = architecture_best_score
#                 best_model_path = best_model_for_architecture_path

#     return best_model_path

# def get_best_model_for_architecture(model_architecture_dir):
#         # For each [4-64-SAGEConv-mean-Adam-0.001, 4-64-SAGEConv-mean-RMSprop-0.001]
    
#         with open(os.path.join(model_architecture_dir, "scores.json")) as f:
#             scores_dict = json.load(f)
        
#         # Get path of the model with the lowest score
#         cur_max = math.inf
#         cur_max_path = None
#         for model_file_name, score in scores_dict.items():
#             if score < cur_max:
#                 cur_max = score
#                 cur_max_path = os.path.join(model_architecture_dir, model_file_name)
        
#         assert cur_max_path is not None, f"No best model found for the model architecture{model_architecture_dir}"

#         return os.path.join(model_architecture_dir,cur_max_path), cur_max



@dataclass
class PreprocessorSettings:
    gnn_retries: int
    gnn_threshold: float
    model_path: str

    def __post_init__(self):
        self.gnn_retries = int(self.gnn_retries)
        self.gnn_threshold = float(self.gnn_threshold)

    @classmethod
    def from_file(cls, file_path) -> "PreprocessorSettings":
        with open(file_path) as f:
            lines = f.read()
            _, gnn_retries, _, gnn_threshold, _, model_path = lines.split(",")
            
        return cls(gnn_retries, gnn_threshold, model_path)

    def to_parameter_string(self):
        return (f"gnn-retries,{self.gnn_retries},"
                f"gnn-threshold,{self.gnn_threshold},"
                f"model-path,{self.model_path}")

@dataclass
class ModelSetting:
    layers_num: int
    hidden_size: int
    conv_type: str
    aggr: str
    optimizer: str
    lr: float
    model_specific_kwargs: dict = field(default_factory=dict)

    def __post_init__(self):
        self.layers_num = int(self.layers_num)
        self.hidden_size = int(self.hidden_size)
        self.lr = float(self.lr)

        self.checks(self.conv_type, CONVOLUTIONS)
        self.checks(self.aggr, AGGREGATIONS)
        self.checks(self.optimizer, OPTIMIZER_CLASSES)

    def from_dict(settings_dict) -> "ModelSetting":
        return ModelSetting(**settings_dict)
    
    @classmethod
    def from_file(cls, path: str):
        with open(os.path.join(path)) as f:
            lines = f.read()
            layers_num, hidden_size, conv_type, aggr, optimizer, lr = lines.split(',')
        
        return cls(layers_num=layers_num, hidden_size=hidden_size, conv_type=conv_type,
                    aggr=aggr, optimizer=optimizer, lr=lr)

    @property
    def dir_name(self):
        """directory of the model setting that will have iteratively trained models"""
        return f"{self.layers_num}-{self.hidden_size}-{self.conv_type}-{self.aggr}-{self.optimizer}-{self.lr}"
    
    def checks(cls, val, allowed_vals):
        if val not in allowed_vals:
            raise ValueError(f"Value {val} not supported.")

    def to_parameter_string(self):
        return (f"layers_num,{self.layers_num},"
                f"hidden_size,{self.hidden_size},"
                f"conv_type,{self.conv_type},"
                f"aggr,{self.aggr},"
                f"optimizer,{self.optimizer},"
                f"lr,{self.lr}")
    
    def to_parameter_string_comma(self):
        return (f"{self.layers_num},"
                f"{self.hidden_size},"
                f"{self.conv_type},"
                f"{self.aggr},"
                f"{self.optimizer},"
                f"{self.lr}")


