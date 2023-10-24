#!/usr/bin/python



import argparse
import os
import subprocess
import sys




parser = argparse.ArgumentParser(description='executes translator on domain/problem/ACTION_FILE, '
                                             'obtains set of actions that are relaxed when only grounding the actions in ACTION_FILE,'
                                             'and writes these actions into OUTFILE_NAME in the folder of domain/problem/ACTION_FILE')
parser.add_argument('--domain', help='name of domain folder in subdominization_data repo', required=True)
parser.add_argument('--output-file-name', help='name of the generated output file', required=True)
parser.add_argument('--actions-file-name', help='name of the generated output file', required=True)
parser.add_argument('--translator-folder', help='where to find the partial grounding translator', required=True)

options = parser.parse_args()

domain = options.domain
translator_dir = options.translator_folder

assert(os.path.isdir("../" + domain))
assert(os.path.isdir(translator_dir))

translate_executable = os.path.abspath(os.path.join(translator_dir, "translate.py"))
assert(os.path.isfile(translate_executable))

outfile_name = options.output_file_name
actions_filename = options.actions_file_name

runs_folder = os.path.join(os.getcwd(), os.pardir, domain, "runs/optimal/") # hard-coded for now; data for all domains has this folder structure

overwrite = False

for task in os.listdir(runs_folder):
    os.chdir(os.path.join(runs_folder, task))
    print(os.getcwd(), end="")
    if (not os.path.isfile(os.path.join(runs_folder, task, actions_filename))):
        print(".. no {actions_filename} file".format(**locals()))
        continue
    
    if (not overwrite and os.path.isfile(os.path.join(runs_folder, task, outfile_name))):
        if (input("  Overwrite {outfile_name}? y,N  ") in ["y", "Y", "Yes", "yes"]):
            overwrite = True
        else:
            sys.exit("abort")
    subprocess.run([translate_executable, 
                        "domain.pddl", 
                        "problem.pddl",
                        "--grounding-action-queue-ordering",
                        "actionsfromfile",
                        "--actions-file",
                        actions_filename,
                        "--reachable-actions-output-file",
                        outfile_name],
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.PIPE,
                    check=True)
    print("... done!")
