#! /usr/bin/env python3

"""
Experiment: Focal Search (cg open / ff focal) vs Eager Search (tiebreaking cg+ff)
Domain: logistics98
"""

import os
import shutil
import subprocess
from pathlib import Path

from downward.experiment import FastDownwardExperiment
from downward.reports.absolute import AbsoluteReport
from downward.reports.scatter import ScatterPlotReport
from lab.environments import LocalEnvironment
from lab.experiment import get_default_data_dir
from lab.reports import Attribute

import project

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
REPO = project.get_repo_base()
BENCHMARKS_DIR = "/home/mat/Investigacion/planning_learning_2/benchmarks/downward-benchmarks"

try:
    REVISION_CACHE = Path(os.environ["DOWNWARD_REVISION_CACHE"])
except KeyError:
    REVISION_CACHE = Path(get_default_data_dir()) / "revision-cache"

# Current HEAD revision of this scorpion checkout
REV = "2f85111d0"

# ---------------------------------------------------------------------------
# Suite & environment
# ---------------------------------------------------------------------------
SUITE = ["logistics98"]

ENV = LocalEnvironment(processes=8)

# ---------------------------------------------------------------------------
# Driver / resource options
# ---------------------------------------------------------------------------
BUILD_OPTIONS = []
DRIVER_OPTIONS = [
    "--overall-time-limit", "10m",
    "--overall-memory-limit", "4096M",
]

# ---------------------------------------------------------------------------
# Algorithm configurations
# ---------------------------------------------------------------------------
# focal_search: cg() as admissible open evaluator, ff() as focal evaluator
# eager: tie-breaking open list with cg() primary, ff() secondary
CONFIGS = [
    (
        "focal-cg-ff",
        ["--search", "focal_search(open_eval=cg(), focal_eval=ff())"],
    ),
    (
        "eager-tiebreak-cg-ff",
        ["--search", "eager(tiebreaking([cg(), ff()]))"],
    ),
]

# ---------------------------------------------------------------------------
# Attributes to report
# ---------------------------------------------------------------------------
ATTRIBUTES = [
    "coverage",
    "error",
    "expansions",
    "expansions_until_last_jump",
    "memory",
    "run_dir",
    "search_time",
    "search_start_time",
    "search_start_memory",
    "total_time",
    project.EVALUATIONS_PER_TIME,
]

# ---------------------------------------------------------------------------
# Build experiment
# ---------------------------------------------------------------------------
exp = FastDownwardExperiment(environment=ENV, revision_cache=REVISION_CACHE)
exp.add_suite(BENCHMARKS_DIR, SUITE)

for config_name, config in CONFIGS:
    exp.add_algorithm(
        config_name,
        REPO,
        REV,
        config,
        driver_options=DRIVER_OPTIONS,
    )

# Add standard FD parsers explicitly (FastDownwardExperiment does not add them automatically).
exp.add_parser(exp.EXITCODE_PARSER)
exp.add_parser(exp.TRANSLATOR_PARSER)
exp.add_parser(exp.SINGLE_SEARCH_PARSER)
exp.add_parser(exp.PLANNER_PARSER)
# Scorpion-specific parser for search_start_time / search_start_memory.
exp.add_parser(project.DIR / "parser.py")

# ---------------------------------------------------------------------------
# Steps
# ---------------------------------------------------------------------------
exp.add_step("build", exp.build)
exp.add_step("start", exp.start_runs)
exp.add_fetcher(name="fetch")

# Absolute report (HTML table with all attributes)
project.add_absolute_report(
    exp,
    name="report-focal-logistics98",
    attributes=ATTRIBUTES,
    filter=project.add_evaluations_per_time,
)

# Scatter plots: focal-cg-ff  vs  eager-tiebreak-cg-ff
ALGO_PAIR = ("focal-cg-ff", "eager-tiebreak-cg-ff")
for attribute in ["total_time", "expansions"]:
    exp.add_report(
        ScatterPlotReport(
            relative=False,
            get_category=lambda run1, run2: run1["domain"],
            attributes=[attribute],
            filter_algorithm=list(ALGO_PAIR),
            filter=project.add_evaluations_per_time,
            format="png",
        ),
        name=f"scatter-focal-vs-eager-{attribute}",
    )

exp.run_steps()
