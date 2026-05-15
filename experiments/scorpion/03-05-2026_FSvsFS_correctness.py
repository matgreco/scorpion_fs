#! /usr/bin/env python3

"""
Experiment: Focal Search correctness check
  - Tiebreaking: f=g+hmax (open) + ff (tiebreak), w=1.0
  - Focal Search: hmax (open) + ff (focal), w=1.0
Domains: first 10 instances of blocks, logistics00, tpp
Date: 03-05-2026
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

REV = "6ad5e90f8"

# ---------------------------------------------------------------------------
# Suite: first 10 instances of each domain
# ---------------------------------------------------------------------------
SUITE = [
    "blocks:probBLOCKS-10-0.pddl",
    "blocks:probBLOCKS-10-1.pddl",
    "blocks:probBLOCKS-10-2.pddl",
    "blocks:probBLOCKS-11-0.pddl",
    "blocks:probBLOCKS-11-1.pddl",
    "blocks:probBLOCKS-11-2.pddl",
    "blocks:probBLOCKS-12-0.pddl",
    "blocks:probBLOCKS-12-1.pddl",
    "blocks:probBLOCKS-13-0.pddl",
    "blocks:probBLOCKS-13-1.pddl",
    "logistics00:probLOGISTICS-10-0.pddl",
    "logistics00:probLOGISTICS-10-1.pddl",
    "logistics00:probLOGISTICS-11-0.pddl",
    "logistics00:probLOGISTICS-11-1.pddl",
    "logistics00:probLOGISTICS-12-0.pddl",
    "logistics00:probLOGISTICS-12-1.pddl",
    "logistics00:probLOGISTICS-13-0.pddl",
    "logistics00:probLOGISTICS-13-1.pddl",
    "logistics00:probLOGISTICS-14-0.pddl",
    "logistics00:probLOGISTICS-14-1.pddl",
    "tpp:p01.pddl",
    "tpp:p02.pddl",
    "tpp:p03.pddl",
    "tpp:p04.pddl",
    "tpp:p05.pddl",
    "tpp:p06.pddl",
    "tpp:p07.pddl",
    "tpp:p08.pddl",
    "tpp:p09.pddl",
    "tpp:p10.pddl",
]

ENV = LocalEnvironment(processes=10)

# ---------------------------------------------------------------------------
# Driver options  (10 min / 8 GB per worker)
# ---------------------------------------------------------------------------
DRIVER_OPTIONS = [
    "--overall-time-limit", "10m",
    "--overall-memory-limit", "8192M",
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
    name="report-FSvsFS-correctness",
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
        name=f"scatter-FSvsFS-correctness-{attribute}",
    )

exp.run_steps()
