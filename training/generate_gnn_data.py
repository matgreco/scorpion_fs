import os 
import sys
from lab.calls.call import Call


ONLY_RELAXED_FLAG = ["--find-relaxed-plan"]
ONLY_SIMPLE_LANDMARKS = ["--find-simple-landmarks"]
BOTH_R_AND_L = ["--find-relaxed-plan", "--find-simple-landmarks"]
NONE_FLAG = []

def run_step_generate_gnn_data(REPO_GNN_LEARNING, PROBLEMS_DIR, OUTPUT_DIR, extra_flags, time_limit=300, memory_limit = 4*1024*1024):
    generation_data_flags = None
    if extra_flags == ONLY_RELAXED_FLAG:
        generation_data_flags = ["--relaxed-plan"]
    elif extra_flags == ONLY_SIMPLE_LANDMARKS:
        generation_data_flags = ["--simple-landmarks"]
    elif extra_flags == BOTH_R_AND_L:
        generation_data_flags = ["--relaxed-plan", "--simple-landmarks"]
    elif extra_flags == NONE_FLAG:
        generation_data_flags = []
    else:
        raise ValueError("Invalid extra flags")


    Call([sys.executable, f'{REPO_GNN_LEARNING}/src/graph_data_generation.py', PROBLEMS_DIR, OUTPUT_DIR] + generation_data_flags, "generate-graphs-gnn" ,time_limit=time_limit, memory_limit=memory_limit).wait()

