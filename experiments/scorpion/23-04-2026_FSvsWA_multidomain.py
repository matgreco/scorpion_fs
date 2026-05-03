#! /usr/bin/env python3

"""
Experiment: Focal Search vs Weighted A* with MS and LMCut heuristics
Algorithms: 8 configs (w=1.5 and w=2.0, FS and WA, ms and lmcut)
Domains: blocks, depot, grid, gripper, logistics00, rubiks-cube-opt23-adl, tpp
         (first 10 easiest problems per domain)
Note: user requested "logistics99" — using logistics00 (IPC-2000, closest match).
Date: 23-04-2026
"""

import os
import re
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
# Suite: first 10 easiest problems per domain (natural numeric sort)
# ---------------------------------------------------------------------------
DOMAINS = [
    "blocks",
    "depot",
    "grid",
    "gripper",
    "logistics00",
    "rubiks-cube-opt23-adl",
    "tpp",
]


def _natural_key(s):
    return [int(c) if c.isdigit() else c.lower() for c in re.split(r"(\d+)", s)]


def get_first_n_problems(domain, n=10):
    domain_dir = Path(BENCHMARKS_DIR) / domain
    problems = sorted(
        [f.name for f in domain_dir.iterdir()
         if f.suffix == ".pddl" and f.name != "domain.pddl"],
        key=_natural_key,
    )
    return [f"{domain}:{p}" for p in problems[:n]]


SUITE = []
for _domain in DOMAINS:
    probs = get_first_n_problems(_domain, 10)
    SUITE.extend(probs)
    print(f"  {_domain}: {len(probs)} problems")

# ---------------------------------------------------------------------------
# Environment: 10 workers in parallel
# ---------------------------------------------------------------------------
ENV = LocalEnvironment(processes=10)

DRIVER_OPTIONS = [
    "--overall-time-limit", "5m",
    "--overall-memory-limit", "8G",
]

# ---------------------------------------------------------------------------
# Merge-and-shrink evaluator (shared across configs that use ms)
# ---------------------------------------------------------------------------
MS_DEF = (
    "ms=merge_and_shrink("
    "merge_strategy=merge_sccs("
    "order_of_sccs=topological,"
    "merge_selector=score_based_filtering("
    "scoring_functions=[goal_relevance(),dfp(),total_order()])),"
    "shrink_strategy=shrink_bisimulation(greedy=false),"
    "label_reduction=exact(before_shrinking=true,before_merging=false),"
    "max_states=50k,"
    "threshold_before_merge=1)"
)

# ---------------------------------------------------------------------------
# Algorithm configurations
# Each entry: (alias, component_options_list)
# ---------------------------------------------------------------------------
CONFIGS = [
    # --- w = 1.5 ---
    (
        "15FS_ms-ff",
        ["--evaluator", MS_DEF,
         "--search", "focal_search(open_eval=sum([g(),ms]), focal_eval=ff(), w=1.5)"],
    ),
    (
        "15FS_lmcut-ff",
        ["--search", "focal_search(open_eval=sum([g(),lmcut()]), focal_eval=ff(), w=1.5)"],
    ),
    (
        "15wa_ms",
        # eager_wastar only accepts int w; express w=1.5 as eager sorted by 2g+3h ∝ g+1.5h
        ["--evaluator", MS_DEF,
         "--search", "eager(single(sum([weight(g(),2),weight(ms,3)])))"],
    ),
    (
        "15wa_lmcut",
        ["--search", "eager(single(sum([weight(g(),2),weight(lmcut(),3)])))"],
    ),
    # --- w = 2.0 ---
    (
        "20FS_ms-ff",
        ["--evaluator", MS_DEF,
         "--search", "focal_search(open_eval=sum([g(),ms]), focal_eval=ff(), w=2.0)"],
    ),
    (
        "20FS_lmcut-ff",
        ["--search", "focal_search(open_eval=sum([g(),lmcut()]), focal_eval=ff(), w=2.0)"],
    ),
    (
        "20wa_ms",
        # w=2 is an integer: eager_wastar works fine here
        ["--evaluator", MS_DEF,
         "--search", "eager_wastar([ms], w=2)"],
    ),
    (
        "20wa_lmcut",
        ["--search", "eager_wastar([lmcut()], w=2)"],
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

# Absolute report with all attributes
project.add_absolute_report(
    exp,
    name="report-FSvsWA-multidomain",
    attributes=ATTRIBUTES,
    filter=project.add_evaluations_per_time,
)

# Scatter plots: one pair per w-value (FS vs WA) for each heuristic
SCATTER_PAIRS = [
    ("15FS_ms-ff",     "15wa_ms",     "w15-ms"),
    ("15FS_lmcut-ff",  "15wa_lmcut",  "w15-lmcut"),
    ("20FS_ms-ff",     "20wa_ms",     "w20-ms"),
    ("20FS_lmcut-ff",  "20wa_lmcut",  "w20-lmcut"),
]

for algo1, algo2, tag in SCATTER_PAIRS:
    for attribute in ["total_time", "expansions"]:
        exp.add_report(
            ScatterPlotReport(
                relative=False,
                get_category=lambda run1, run2: run1["domain"],
                attributes=[attribute],
                filter_algorithm=[algo1, algo2],
                filter=project.add_evaluations_per_time,
                format="png",
            ),
            name=f"scatter-{tag}-{attribute}",
        )

exp.run_steps()
