#! /usr/bin/env python3

"""
Experiment: Focal Search w=1 (f=g+hmax open / ff focal) vs Eager (tiebreaking f=g+hmax + ff)
Domain: logistics98
Date: 23-04-2026
"""

import os
from pathlib import Path

from downward.experiment import FastDownwardExperiment
from downward.reports.absolute import AbsoluteReport
from downward.reports.scatter import ScatterPlotReport
from lab.environments import LocalEnvironment
from lab.experiment import get_default_data_dir

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

REV = "2f85111d0"

# ---------------------------------------------------------------------------
# Suite & environment
# ---------------------------------------------------------------------------
SUITE = ["logistics98"]

ENV = LocalEnvironment(processes=8)

# ---------------------------------------------------------------------------
# Driver options  (10 min / 4 GB per worker)
# ---------------------------------------------------------------------------
DRIVER_OPTIONS = [
    "--overall-time-limit", "10m",
    "--overall-memory-limit", "4096M",
]

# ---------------------------------------------------------------------------
# Algorithm configurations
# ---------------------------------------------------------------------------
CONFIGS = [
    (
        "eager-tiebreak-gHmax-ff",
        ["--search", "eager(tiebreaking([sum([g(),hmax()]),ff()]))"],
    ),
    (
        "focal-w1-gHmax-ff",
        ["--search", "focal_search(open_eval=sum([g(),hmax()]), focal_eval=ff(), w=1.0)"],
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

exp.add_parser(exp.EXITCODE_PARSER)
exp.add_parser(exp.TRANSLATOR_PARSER)
exp.add_parser(exp.SINGLE_SEARCH_PARSER)
exp.add_parser(exp.PLANNER_PARSER)
exp.add_parser(project.DIR / "parser.py")

# ---------------------------------------------------------------------------
# Steps
# ---------------------------------------------------------------------------
exp.add_step("build", exp.build)
exp.add_step("start", exp.start_runs)
exp.add_fetcher(name="fetch")

ALGO_PAIR = ("focal-w1-gHmax-ff", "eager-tiebreak-gHmax-ff")

project.add_absolute_report(
    exp,
    name="report-FSvsTiebreaking-hmax",
    attributes=ATTRIBUTES,
    filter=project.add_evaluations_per_time,
)

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
        name=f"scatter-FSvsTiebreaking-hmax-{attribute}",
    )

exp.run_steps()
