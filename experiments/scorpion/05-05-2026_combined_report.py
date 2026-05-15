#! /usr/bin/env python3

"""
Combined report: FSvsWA opt-track + FScea opt-track
Merges properties from both experiments and generates:
  - AbsoluteReport (all 18 configs)
  - Coverage plots per weight (time + expansions)
  - Scatter plots: FS-CEA vs FS-ff and FS-CEA vs WA* for each (w, heuristic)
Date: 05-05-2026
"""

import json
import sys
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.lines as mlines

# ── Paths ────────────────────────────────────────────────────────────────────
SCORPION_EXPERIMENTS = Path(__file__).parent
DATA_DIR   = SCORPION_EXPERIMENTS / "data"
FSWA_EVAL  = DATA_DIR / "05-05-2026_FSvsWA_opt_slurm-eval"
CEA_EVAL   = DATA_DIR / "05-05-2026_FScea_opt_slurm-eval"
COMBINED   = DATA_DIR / "05-05-2026_combined_opt_eval"
COMBINED.mkdir(exist_ok=True)

# ── Merge properties ──────────────────────────────────────────────────────────
print("Loading FSvsWA properties...")
with open(FSWA_EVAL / "properties") as f:
    props_fswa = json.load(f)

print("Loading CEA properties...")
with open(CEA_EVAL / "properties") as f:
    props_cea = json.load(f)

print(f"  FSvsWA runs: {len(props_fswa)}")
print(f"  CEA runs:    {len(props_cea)}")

# Check for key collisions (there should be none since different algorithms)
overlap = set(props_fswa) & set(props_cea)
if overlap:
    print(f"WARNING: {len(overlap)} overlapping keys — CEA values will overwrite FSvsWA.")

merged = {**props_fswa, **props_cea}
print(f"  Merged runs: {len(merged)}")

with open(COMBINED / "properties", "w") as f:
    json.dump(merged, f)
print(f"Saved merged properties to {COMBINED / 'properties'}")

# ── Lab AbsoluteReport ────────────────────────────────────────────────────────
try:
    from downward.reports.absolute import AbsoluteReport
    from lab.experiment import Experiment
    from lab.reports import Report

    # We use lab's report machinery directly on the merged properties file.
    import sys
    sys.path.insert(0, str(SCORPION_EXPERIMENTS))
    import project

    ATTRIBUTES = [
        "coverage",
        "error",
        "expansions",
        "expansions_until_last_jump",
        "memory",
        "search_time",
        "total_time",
        project.EVALUATIONS_PER_TIME,
    ]

    report = AbsoluteReport(attributes=ATTRIBUTES, filter=project.add_evaluations_per_time)
    outfile = COMBINED / "report-combined-opt.html"
    report(str(COMBINED / "properties"), str(outfile))
    print(f"Saved HTML report: {outfile}")
except Exception as e:
    print(f"Lab report failed ({e}). Skipping HTML report.")

# ── Coverage plots ────────────────────────────────────────────────────────────
ALL_CONFIGS = [
    # FSvsWA configs
    "15FS_ms-ff",   "15FS_lmcut-ff",  "15wa_ms",       "15wa_lmcut",
    "20FS_ms-ff",   "20FS_lmcut-ff",  "20wa_ms",       "20wa_lmcut",
    "30FS_ms-ff",   "30FS_lmcut-ff",  "30wa_ms",       "30wa_lmcut",
    # CEA configs
    "15FS_ms-cea",  "15FS_lmcut-cea",
    "20FS_ms-cea",  "20FS_lmcut-cea",
    "30FS_ms-cea",  "30FS_lmcut-cea",
]

solved_times  = {c: [] for c in ALL_CONFIGS}
solved_expans = {c: [] for c in ALL_CONFIGS}

for run in merged.values():
    alg = run.get("algorithm")
    if alg not in ALL_CONFIGS:
        continue
    if run.get("coverage") == 1:
        t = run.get("total_time")
        e = run.get("expansions")
        if t is not None:
            solved_times[alg].append(t)
        if e is not None:
            solved_expans[alg].append(e)

for c in ALL_CONFIGS:
    solved_times[c].sort()
    solved_expans[c].sort()

print("\nSolved per config:")
for c in ALL_CONFIGS:
    print(f"  {c}: {len(solved_times[c])}")

# ── Style ──────────────────────────────────────────────────────────────────────
MS_COLOR    = "#2a77cc"
LMCUT_COLOR = "#d06000"
CEA_MARK    = "^"   # triangle marker for CEA configs

def style(cfg):
    is_ms  = "ms"  in cfg
    is_fs  = "FS"  in cfg
    is_cea = "cea" in cfg
    is_wa  = cfg.startswith(("15wa","20wa","30wa"))
    color  = MS_COLOR if is_ms else LMCUT_COLOR
    if is_wa:
        ls, lw = "--", 1.8
    elif is_cea:
        ls, lw = "-.",  2.0
    else:
        ls, lw = "-",   2.2
    h_str  = "MS" if is_ms else "LMCut"
    focal  = "CEA" if is_cea else "ff"
    alg    = "WA*" if is_wa else f"FS({focal})"
    label  = f"{alg}({h_str})"
    return color, ls, lw, label

def coverage_curve(sorted_vals):
    xs = [0.0] + sorted_vals
    ys = list(range(len(xs)))
    return xs, ys

GROUPS = {
    "w=1.5": [c for c in ALL_CONFIGS if c.startswith("15")],
    "w=2.0": [c for c in ALL_CONFIGS if c.startswith("20")],
    "w=3.0": [c for c in ALL_CONFIGS if c.startswith("30")],
}

# Legend handles
fs_ff_line  = mlines.Line2D([], [], color="gray", ls="-",   lw=2.2, label="FS(ff)")
fs_cea_line = mlines.Line2D([], [], color="gray", ls="-.",  lw=2.0, label="FS(CEA)")
wa_line     = mlines.Line2D([], [], color="gray", ls="--",  lw=1.8, label="WA*")
ms_line     = mlines.Line2D([], [], color=MS_COLOR,    ls="-", lw=6, alpha=0.4, label="MS")
lm_line     = mlines.Line2D([], [], color=LMCUT_COLOR, ls="-", lw=6, alpha=0.4, label="LMCut")
style_handles = [fs_ff_line, fs_cea_line, wa_line, ms_line, lm_line]

for w_label, cfgs in GROUPS.items():
    fig, axes = plt.subplots(1, 2, figsize=(13, 5))
    fig.suptitle(f"Coverage — {w_label} (all focal evaluators)", fontsize=13, fontweight="bold")

    for ax, (data_dict, xlabel, xscale, xlim) in zip(axes, [
        (solved_times,  "Time (s)",   "linear", (0, 620)),
        (solved_expans, "Expansions", "log",     None),
    ]):
        for cfg in cfgs:
            vals = data_dict[cfg]
            if not vals:
                continue
            color, ls, lw, label = style(cfg)
            xs, ys = coverage_curve(vals)
            ax.step(xs, ys, where="post", color=color, linestyle=ls,
                    linewidth=lw, label=label, alpha=0.92)

        ax.set_xlabel(xlabel, fontsize=12)
        ax.set_ylabel("# Solved Problems", fontsize=12)
        ax.set_xscale(xscale)
        if xlim:
            ax.set_xlim(xlim)
        ax.set_ylim(bottom=0)
        ax.grid(True, alpha=0.3, linestyle=":")

        handles, labels = [], []
        for cfg in cfgs:
            color, ls, lw, label = style(cfg)
            handles.append(mlines.Line2D([], [], color=color, ls=ls, lw=lw, label=label))
            labels.append(label)
        ax.legend(handles, labels, fontsize=9, loc="lower right", framealpha=0.9)

    axes[0].add_artist(
        axes[0].legend(style_handles, [h.get_label() for h in style_handles],
                       fontsize=9, loc="upper left", framealpha=0.9,
                       title="Style guide", title_fontsize=8)
    )
    # Re-add config legend for axes[0]
    h2, l2 = [], []
    for cfg in cfgs:
        color, ls, lw, label = style(cfg)
        h2.append(mlines.Line2D([], [], color=color, ls=ls, lw=lw, label=label))
        l2.append(label)
    axes[0].legend(h2, l2, fontsize=9, loc="lower right", framealpha=0.9)

    tag = {"w=1.5": "15", "w=2.0": "20", "w=3.0": "30"}[w_label]
    out = COMBINED / f"coverage_combined_w{tag}.png"
    fig.tight_layout()
    fig.savefig(out, dpi=150, bbox_inches="tight")
    plt.close(fig)
    print(f"Saved: {out}")

print("\nDone. All outputs in:", COMBINED)
