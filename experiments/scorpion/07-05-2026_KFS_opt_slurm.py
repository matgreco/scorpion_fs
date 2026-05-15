#! /usr/bin/env python3

"""
Experiment: K-Focal Search — opt-track domains
Algorithms : KFS with (MS or LMCut) open evaluator and ff focal evaluator
             w = {1.5, 2.0, 3.0}  x  K = {5, 10, 20, 30}  (24 configurations)
Domains    : 24 IPC opt-track domains (IPC 2014 & 2018)
Cluster    : UAI (SLURM), 80 simultaneous jobs, 10 min / 4 GB each
Date       : 07-05-2026
"""

import os
import re
from pathlib import Path

from downward.experiment import FastDownwardExperiment
from downward.reports.absolute import AbsoluteReport
from lab.environments import SlurmEnvironment
from lab.experiment import get_default_data_dir

import project

# ---------------------------------------------------------------------------
# Custom UAI SLURM environment
# ---------------------------------------------------------------------------
class UAISlurmEnvironment(SlurmEnvironment):
    MAX_TASKS = 1000

    def _get_job_header(self, step, is_last):
        header = super()._get_job_header(step, is_last)
        header = re.sub(r"(#SBATCH --array=\d+-\d+)", r"\1%100", header)
        return header

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
REPO = Path("~/planning/scorpion_fs").expanduser()
BENCHMARKS_DIR = str(Path("~/planning/downward-benchmarks").expanduser())

try:
    REVISION_CACHE = Path(os.environ["DOWNWARD_REVISION_CACHE"])
except KeyError:
    REVISION_CACHE = Path(get_default_data_dir()) / "revision-cache"

REV = "e54e30be6"

# ---------------------------------------------------------------------------
# Suite: 24 IPC opt-track domains (same as FSvsWA opt experiment)
# ---------------------------------------------------------------------------
DOMAINS = [
    "barman-opt14-strips",
    "cavediving-14-adl",
    "childsnack-opt14-strips",
    "citycar-opt14-adl",
    "floortile-opt14-strips",
    "ged-opt14-strips",
    "hiking-opt14-strips",
    "maintenance-opt14-adl",
    "openstacks-opt14-strips",
    "parking-opt14-strips",
    "tetris-opt14-strips",
    "tidybot-opt11-strips",
    "transport-opt14-strips",
    "visitall-opt14-strips",
    "agricola-opt18-strips",
    "caldera-opt18-adl",
    "data-network-opt18-strips",
    "nurikabe-opt18-adl",
    "organic-synthesis-opt18-strips",
    "petri-net-alignment-opt18-strips",
    "settlers-opt18-adl",
    "snake-opt18-strips",
    "spider-opt18-strips",
    "termes-opt18-strips",
]

SUITE = DOMAINS

# ---------------------------------------------------------------------------
# Environment  (80 simultaneous, ~11 runs/task × 10 min = ~110 min)
# ---------------------------------------------------------------------------
ENV = UAISlurmEnvironment(
    partition="compute-slim",
    memory_per_cpu="4G",
    time_limit_per_task="2:10:00",
)

DRIVER_OPTIONS = [
    "--overall-time-limit", "10m",
    "--overall-memory-limit", "4096M",
]

# ---------------------------------------------------------------------------
# Merge-and-shrink evaluator definition
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
# Configurations: KFS  w ∈ {1.5, 2.0, 3.0}  ×  K ∈ {5, 10, 20, 30}
#                open  = admissible (MS or LMCut)
#                focal = ff
# ---------------------------------------------------------------------------
WEIGHTS = [(1.5, "15"), (2.0, "20"), (3.0, "30")]
KS = [5, 10, 20, 30]

CONFIGS = []
for w, wtag in WEIGHTS:
    for k in KS:
        CONFIGS.append((
            f"{wtag}KFS{k}_ms-ff",
            ["--evaluator", MS_DEF,
             "--search", f"focal_search(open_eval=sum([g(),ms]), focal_eval=ff(), w={w}, k={k})"],
        ))
        CONFIGS.append((
            f"{wtag}KFS{k}_lmcut-ff",
            ["--search", f"focal_search(open_eval=sum([g(),lmcut()]), focal_eval=ff(), w={w}, k={k})"],
        ))

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
    name="report-KFS-opt",
    attributes=ATTRIBUTES,
    filter=project.add_evaluations_per_time,
)

exp.run_steps()
