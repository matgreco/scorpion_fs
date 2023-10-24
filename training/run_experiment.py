import shutil

from lab import environments, tools
from lab.experiment import Experiment, get_default_data_dir, Run
from downward.experiment import FastDownwardExperiment

import os
from downward.experiment import (
    _DownwardAlgorithm,
    _get_solver_resource_name,
    FastDownwardRun,
)

from lab.steps import get_step, get_steps_text, Step

from dataclasses import dataclass
from typing import List
from pathlib import Path

import process_lab_dir

@dataclass
class MockCachedRevision:
    name: str
    repo: str
    local_rev: str
    global_rev: str
    build_options: List[str]

class RunExperiment:

    def __init__(self, time_limit, memory_limit):
        self.time_limit = time_limit
        self.memory_limit = memory_limit

    def run (self, path_exp, cached_rev, planner, config, ENV, SUITE, build_options = [], driver_options = [], extra_parsers = []):

        exp = Experiment(path=path_exp+ "-exp", environment=ENV)

        PLANNER = Path (planner)

        exp.add_resource("", PLANNER / "driver", "code/driver")
        exp.add_resource(_get_solver_resource_name(cached_rev), PLANNER / "fast-downward.py", "code/fast-downward.py")
        exp.add_resource("", PLANNER / "builds" / "release64" / "bin", "code/builds/release64/bin")

        for task in SUITE:
            algo = _DownwardAlgorithm(
                f"name",
                cached_rev,
                driver_options + ["--overall-time-limit", self.time_limit, "--overall-memory-limit", self.memory_limit],
                config,
            )
            run = FastDownwardRun(exp, algo, task)
            exp.add_run(run)

        exp.add_parser(FastDownwardExperiment.EXITCODE_PARSER)
        exp.add_parser(FastDownwardExperiment.SINGLE_SEARCH_PARSER)
        exp.add_parser(FastDownwardExperiment.TRANSLATOR_PARSER)
        exp.add_parser(FastDownwardExperiment.PLANNER_PARSER)

        for parser in extra_parsers:
            exp.add_parser(parser)

        exp.add_step("build", exp.build)
        exp.add_step("start", exp.start_runs)

        ENV.run_steps(exp.steps)

        process_lab_dir.process_lab_dir(path_exp+ "-exp", path_exp)
        shutil.rmtree(path_exp+ "-exp")


    def run_planner (self, path_exp, planner, config, ENV, SUITE, build_options = [], driver_options = []):
        rev = "ipc2023-classical"
        cached_rev = MockCachedRevision(name='planner', repo=planner, local_rev='default', global_rev=None, build_options=build_options)

        self.run(path_exp, cached_rev, planner, config, ENV, SUITE, build_options=build_options, driver_options=driver_options, extra_parsers =[f"{os.path.dirname(__file__)}/parsers/goodops-parser.py"] )


    def run_good_operators(self, path_exp, planner, config, ENV, SUITE, build_options = [], driver_options = []):
        rev = "ipc2023-classical"
        cached_rev = MockCachedRevision(name='good_operators', repo=planner, local_rev='default', global_rev=None, build_options=build_options)

        self.run(path_exp, cached_rev, planner,config, ENV, SUITE, build_options=build_options, driver_options=driver_options, extra_parsers =[f"{os.path.dirname(__file__)}/parsers/goodops-parser.py"] )
