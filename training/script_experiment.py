#! /usr/bin/env python

"""
Runs a script on each task of the benchmark suite
"""

import os
import platform

from downward import suites
from lab.experiment import Experiment

class ScriptExperiment (Experiment):
    def __init__(self, path, script_name, script_path, parameters, TIME_LIMIT, MEMORY_LIMIT):
        Experiment.__init__(self, path=path)

        self.add_resource(script_name, script_path, symlink=True)

        for task in suites.build_suite(BENCHMARKS_DIR, SUITE):
            run = self.add_run()

            run.add_resource("domain", task.domain_file, symlink=True)
            run.add_resource("problem", task.problem_file, symlink=True)

            run.add_command(
                "run-script"
                [script_name] + parameters,
                time_limit=TIME_LIMIT,
                memory_limit=MEMORY_LIMIT,
            )
            run.set_property("domain", task.domain)
            run.set_property("problem", task.problem)
            run.set_property("algorithm", "ff")

            run.set_property("time_limit", TIME_LIMIT)
            run.set_property("memory_limit", MEMORY_LIMIT)

            run.set_property("id", [task.domain, task.problem])
