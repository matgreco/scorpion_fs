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


def run_step_good_operators(path_exp, planner, config, ENV, SUITE, fetch_everything=False, build_options = [], driver_options = ["--overall-time-limit", "10"], extra_resources = []):
    exp = Experiment(path=path_exp+ "-exp", environment=ENV)

    rev = "ipc2023-classical"
    cached_rev = MockCachedRevision(name='good_operators', repo=planner, local_rev='default', global_rev=None, build_options=build_options)

    PLANNER = Path (planner)

    exp.add_resource("", PLANNER / "driver", "code/driver")
    exp.add_resource(_get_solver_resource_name(cached_rev), PLANNER / "fast-downward.py", "code/fast-downward.py")
    exp.add_resource("", PLANNER / "builds" / "release64" / "bin", "code/builds/release64/bin")

    for task in SUITE:
        algo = _DownwardAlgorithm(
            f"name",
            cached_rev,
            driver_options,
            config,
        )
        run = FastDownwardRun(exp, algo, task)
        exp.add_run(run)

    exp.add_parser(FastDownwardExperiment.EXITCODE_PARSER)
    exp.add_parser(FastDownwardExperiment.SINGLE_SEARCH_PARSER)
    exp.add_parser(FastDownwardExperiment.TRANSLATOR_PARSER)
    exp.add_parser(FastDownwardExperiment.PLANNER_PARSER)

    exp.add_parser(f"{os.path.dirname(__file__)}/parsers/goodops-parser.py")

    exp.add_step("build", exp.build)
    exp.add_step("start", exp.start_runs)

    ENV.run_steps(exp.steps)

    process_lab_dir.process_lab_dir(path_exp+ "-exp", path_exp)
    shutil.rmtree(path_exp+ "-exp")



    #     exp.add_algorithm(f"good-operators-{name}", planner, revision, config, build_options, driver_options)
    #     exp.add_suite (self.benchmark_folder, self.instances)


    # self.add_substep(f"build-good-operators-{name}", exp, exp.build)
    # self.add_substep(f"start-good-operators-{name}", exp, exp.start_runs)
    # self.add_step(f"fetch-good-operators-{name}", self.fetch_good_operators, path_exp, name, fetch_everything)

    # self.add_fetcher(path_exp, name=f"fetch-goodops{name}-properties",merge=None if  fetch_everything else True)



# class GoodOperatorsExperiment(FastDownwardExperiment):
#     def __init__(self, path=None, environment=None, resources_path=None,extra_resources=[]):
#         FastDownwardExperiment.__init__(self, path=path, environment=environment)
#         self.resources_path= resources_path
#         self.extra_resources = extra_resources

#     # def build(self, **kwargs):
#     #     """Add Fast Downward code, runs and write everything to disk.

#     #     This method is called by the second experiment step.

#     #     """
#     #     if not self._algorithms:
#     #         logging.critical("You must add at least one algorithm.")

#     #     # We convert the problems in suites to strings to avoid errors when converting
#     #     # properties to JSON later. The clean but more complex solution would be to add
#     #     # a method to the JSONEncoder that recognizes and correctly serializes the class
#     #     # Problem.
#     #     serialized_suites = {
#     #         benchmarks_dir: [str(problem) for problem in benchmarks]
#     #         for benchmarks_dir, benchmarks in self._suites.items()
#     #     }
#     #     self.set_property("suite", serialized_suites)
#     #     self.set_property("algorithms", list(self._algorithms.keys()))

#     #     self._add_code()
#     #     self._add_runs()

#     #     if self.extra_resources:
#     #         for run in self.runs:
#     #             for resource in self.extra_resources:
#     #                 resource_name = resource
#     #                 resource_filename = resource
#     #                 task_name = run.properties["problem"].replace('.pddl','')
#     #                 resource_file = f'{self.resources_path}/{task_name}/{resource_filename}'

#     #                 if os.path.exists(resource_file):
#     #                     run.add_resource(
#     #                         resource_name, resource_file, resource_filename, symlink=True
#     #                     )
#     #                 else:
#     #                     print("Warning: missing resource: {resource_name}")

#     #     Experiment.build(self, **kwargs)


#     def add_algorithm(
#         self,
#         name,
#         repo,
#         rev,
#         component_options,
#         build_options=None,
#         driver_options=None,
#     ):
#         if not isinstance(name, str):
#             logging.critical(f"Algorithm name must be a string: {name}")
#         if name in self._algorithms:
#             logging.critical(f"Algorithm names must be unique: {name}")
#         build_options = build_options or []
#         driver_options = [
#             "--validate",
#             "--overall-time-limit",
#             "30m",
#             "--overall-memory-limit",
#             "3584M",
#         ] + (driver_options or [])
#         algorithm = _DownwardAlgorithm(
#             name,
#             CachedFastDownwardRevision(repo, rev, build_options),
#             driver_options,
#             component_options,
#         )
#         for algo in self._algorithms.values():
#             if algorithm == algo:
#                 logging.critical(
#                     f"Algorithms {algo.name} and {algorithm.name} are identical."
#                 )
#         self._algorithms[name] = algorithm
