#! /usr/bin/env python3

"""
Experiment: Focal Search vs Weighted A* — multi-domain, multi-weight
Algorithms : FS and WA* with merge-and-shrink or lmcut, at w=1.5, 2.0, 3.0
             (12 configurations total)
Domains    : 24 IPC sat-track domains, all 20 instances each (480 tasks)
Date       : 03-05-2026
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
# Suite: all 20 instances of each domain (sat variants; petri-net has opt only)
# ---------------------------------------------------------------------------
DOMAINS = [
    "barman-sat14-strips",
    "cavediving-14-adl",
    "childsnack-sat14-strips",
    "citycar-sat14-adl",
    "floortile-sat14-strips",
    "ged-sat14-strips",
    "hiking-sat14-strips",
    "maintenance-sat14-adl",
    "openstacks-sat14-strips",
    "parking-sat14-strips",
    "tetris-sat14-strips",
    "tidybot-sat11-strips",
    "transport-sat14-strips",
    "visitall-sat14-strips",
    "agricola-sat18-strips",
    "caldera-sat18-adl",
    "data-network-sat18-strips",
    "nurikabe-sat18-adl",
    "organic-synthesis-sat18-strips",
    "petri-net-alignment-opt18-strips",
    "settlers-sat18-adl",
    "snake-sat18-strips",
    "spider-sat18-strips",
    "termes-sat18-strips",
]

SUITE = DOMAINS  # lab expands domain names to all their problems automatically

# ---------------------------------------------------------------------------
# Environment: 10 workers, 10 min / 4 GB each
# ---------------------------------------------------------------------------
ENV = LocalEnvironment(processes=10)

DRIVER_OPTIONS = [
    "--overall-time-limit", "10m",
    "--overall-memory-limit", "4096M",
]

# ---------------------------------------------------------------------------
# Merge-and-shrink evaluator definition (reused across configs)
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
#
# WA* with integer w: eager_wastar([h], w=N)
# WA* with w=1.5   : eager(single(sum([weight(g(),2),weight(h,3)])))
#                    minimises 2g+3h ∝ g+1.5h
# ---------------------------------------------------------------------------
CONFIGS = [
    # ── w = 1.5 ──────────────────────────────────────────────────────────
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
        ["--evaluator", MS_DEF,
         "--search", "eager(single(sum([weight(g(),2),weight(ms,3)])))"],
    ),
    (
        "15wa_lmcut",
        ["--search", "eager(single(sum([weight(g(),2),weight(lmcut(),3)])))"],
    ),
    # ── w = 2.0 ──────────────────────────────────────────────────────────
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
        ["--evaluator", MS_DEF,
         "--search", "eager_wastar([ms], w=2)"],
    ),
    (
        "20wa_lmcut",
        ["--search", "eager_wastar([lmcut()], w=2)"],
    ),
    # ── w = 3.0 ──────────────────────────────────────────────────────────
    (
        "30FS_ms-ff",
        ["--evaluator", MS_DEF,
         "--search", "focal_search(open_eval=sum([g(),ms]), focal_eval=ff(), w=3.0)"],
    ),
    (
        "30FS_lmcut-ff",
        ["--search", "focal_search(open_eval=sum([g(),lmcut()]), focal_eval=ff(), w=3.0)"],
    ),
    (
        "30wa_ms",
        ["--evaluator", MS_DEF,
         "--search", "eager_wastar([ms], w=3)"],
    ),
    (
        "30wa_lmcut",
        ["--search", "eager_wastar([lmcut()], w=3)"],
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

project.add_absolute_report(
    exp,
    name="report-FSvsWA-multidomain",
    attributes=ATTRIBUTES,
    filter=project.add_evaluations_per_time,
)

# Scatter plots: FS vs WA for each (w, heuristic) pair
SCATTER_PAIRS = [
    ("15FS_ms-ff",    "15wa_ms",    "w15-ms"),
    ("15FS_lmcut-ff", "15wa_lmcut", "w15-lmcut"),
    ("20FS_ms-ff",    "20wa_ms",    "w20-ms"),
    ("20FS_lmcut-ff", "20wa_lmcut", "w20-lmcut"),
    ("30FS_ms-ff",    "30wa_ms",    "w30-ms"),
    ("30FS_lmcut-ff", "30wa_lmcut", "w30-lmcut"),
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
