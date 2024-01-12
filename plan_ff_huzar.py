#! /usr/bin/env python

from __future__ import print_function

import argparse
import os
import os.path
import subprocess
import sys
import shutil
import tarfile
from datetime import datetime

NONE_FLAG = []
extra_flags = NONE_FLAG

ROOT = os.path.dirname(os.path.abspath(__file__))

def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("domain_knowledge", help="path to domain knowledge file")
    parser.add_argument("domain", help="path to domain file")
    parser.add_argument("problem", help="path to problem file")
    parser.add_argument("plan", help="path to output plan file")
    parser.add_argument("time_limit")
    parser.add_argument("memory_limit")
    parser.add_argument("--priority", help="Type of priority", default="path")
    
    return parser.parse_args()


def main():
    args = parse_args()

    # best_model_path = os.join.path(args.dk, "best_model/model.pt")

    ROOT = os.path.dirname(os.path.abspath(__file__))
    REPO_GNN_LEARNING = f"{ROOT}/gnn-learning"
    SCORPION_PATH = f"{ROOT}"

    DK_DIR_FILE = args.domain_knowledge
    DOMAIN = args.domain
    PROBLEM = args.problem
    PLAN_OUT = args.plan
    priority_type = args.priority

    assert(priority_type in ["instant", "path", "path_norm"])

    dk_folder = f"extracted"

    # uncompress domain knowledge file
    with tarfile.open(args.domain_knowledge, "r:gz") as tar:
        if os.path.exists(dk_folder):
            shutil.rmtree(dk_folder)
        tar.extractall(dk_folder)


    
    # DK_DIR = f'{ROOT}/DK'

    # if os.path.exists(DK_DIR):
    #     shutil.rmtree(DK_DIR)
    # os.mkdir(DK_DIR)

    # shutil.unpack_archive(DK_DIR_FILE, DK_DIR ,'zip')
    
    model_path = os.path.join(dk_folder, "model.pt")
    # preprocessor_settings = os.path.join(dk_folder, "DK", "preprocessor_settings.txt")
    preprocessor_settings = "gnn-retries,2,gnn-threshold,0.0,model-path,extracted/DK/model.pt" # with threshold = 0.0 keep all operators in the SAS file

    print("Current Path:", os.getcwd())
    print("Repo GNN Learning", REPO_GNN_LEARNING)
    """
    subprocess.check_call(
        [f'{SCORPION_PATH}/fast-downward.py']
        + extra_flags + [
            '--alias', 'lama-first',
            '--keep-sas-file',
            '--transform-task-options', preprocessor_settings,
            '--transform-task', f'{REPO_GNN_LEARNING}/src/preprocessor.command',
            '--overall-time-limit', '2400',
            '--search-time-limit', '500',
            DOMAIN,
            PROBLEM])
    """
    
    subprocess.check_call(
        [f'{SCORPION_PATH}/fast-downward.py']
        + extra_flags + [
            '--keep-sas-file',
            '--transform-task-options', preprocessor_settings,
            '--transform-task', f'{REPO_GNN_LEARNING}/src/preprocessor.command',
            '--overall-time-limit', args.time_limit,
            '--search-time-limit', args.time_limit,
            '--overall-memory-limit', args.memory_limit,
            DOMAIN,
            PROBLEM,
            '--evaluator', f'opp=operator_priorities(priority={priority_type}())',
            '--search', "eager_greedy([opp, ff()])",
            ])
#priority could be [instant, path, path_norm]
#./fast-downward.py benchmarks/blocksworld/domain.pddl benchmarks/blocksworld/training/easy/p10.pddl 
# --evaluator "hprio=opprio()" --search "eager_greedy([hprio])"

#

if __name__ == "__main__":
    # # Get the current date and time
    # current_datetime = datetime.now()

    # # Format the datetime as per your requirement
    # formatted_datetime = current_datetime.strftime("%Y-%m-%dT%H:%M:%S.%f")[:-3]
    # print(f"Start time: {formatted_datetime}")

    main()
