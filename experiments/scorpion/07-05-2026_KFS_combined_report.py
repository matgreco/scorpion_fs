#! /usr/bin/env python3

"""
Combined report: K-Focal Search opt-track + WA* opt-track
Merges KFS properties (24 configs) with WA* properties from FSvsWA experiment
and generates:
  - AbsoluteReport (all 30 configs: 24 KFS + 6 WA*)
  - Coverage plots per weight: KFS K={5,10,20,30} vs WA* (time + expansions)
Date: 07-05-2026
"""

import json
import sys
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.lines as mlines

# ── Paths ─────────────────────────────────────────────────────────────────────
SCORPION_EXPERIMENTS = Path(__file__).parent
DATA_DIR  = SCORPION_EXPERIMENTS / "data"
KFS_EVAL  = DATA_DIR / "07-05-2026_KFS_opt_slurm-eval"
FSWA_EVAL = DATA_DIR / "05-05-2026_FSvsWA_opt_slurm-eval"
COMBINED  = DATA_DIR / "07-05-2026_KFS_combined_opt_eval"
COMBINED.mkdir(exist_ok=True)

# ── Merge properties ──────────────────────────────────────────────────────────
print("Loading KFS properties...")
with open(KFS_EVAL / "properties") as f:
    props_kfs = json.load(f)

print("Loading FSvsWA (WA*) properties...")
with open(FSWA_EVAL / "properties") as f:
    props_fswa = json.load(f)

print(f"  KFS runs:   {len(props_kfs)}")
print(f"  FSvsWA runs:{len(props_fswa)}")

# Keep only WA* configs from the FSvsWA properties
WA_CONFIGS = {
    "15wa_ms", "15wa_lmcut",
    "20wa_ms", "20wa_lmcut",
    "30wa_ms", "30wa_lmcut",
}
props_wa = {k: v for k, v in props_fswa.items() if v.get("algorithm") in WA_CONFIGS}
print(f"  WA* runs kept: {len(props_wa)}")

overlap = set(props_kfs) & set(props_wa)
if overlap:
    print(f"WARNING: {len(overlap)} overlapping keys — WA* values will overwrite KFS.")

merged = {**props_kfs, **props_wa}
print(f"  Merged runs: {len(merged)}")

with open(COMBINED / "properties", "w") as f:
    json.dump(merged, f)
print(f"Saved merged properties to {COMBINED / 'properties'}")

# ── Lab AbsoluteReport ────────────────────────────────────────────────────────
try:
    from downward.reports.absolute import AbsoluteReport
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
    outfile = COMBINED / "report-KFS-combined-opt.html"
    report(str(COMBINED), str(outfile))
    print(f"Saved HTML report: {outfile}")
except Exception as e:
    print(f"Lab report failed ({e}). Skipping HTML report.")

# ── Collect coverage data ─────────────────────────────────────────────────────
WEIGHTS  = [("15", "w=1.5"), ("20", "w=2.0"), ("30", "w=3.0")]
KS       = [5, 10, 20, 30]
HEURS    = ["ms", "lmcut"]

KFS_CONFIGS = [f"{wtag}KFS{k}_{h}-ff"
               for wtag, _ in WEIGHTS for k in KS for h in HEURS]
ALL_CONFIGS  = KFS_CONFIGS + list(WA_CONFIGS)

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

K_STYLES = {
    5:  ("-",   2.0),
    10: ("--",  2.0),
    20: ("-.",  2.0),
    30: (":",   2.2),
}

def style(cfg):
    is_ms = "ms" in cfg and "lmcut" not in cfg
    color = MS_COLOR if is_ms else LMCUT_COLOR
    h_str = "MS" if is_ms else "LMCut"

    if cfg.startswith(("15wa", "20wa", "30wa")):
        return color, "--", 2.5, f"WA*({h_str})"

    # KFS config: {wtag}KFS{k}_{h}-ff
    for k in KS:
        if f"KFS{k}_" in cfg:
            ls, lw = K_STYLES[k]
            return color, ls, lw, f"KFS-{k}({h_str})"

    return color, "-", 1.5, cfg  # fallback

def coverage_curve(sorted_vals):
    xs = [0.0] + sorted_vals
    ys = list(range(len(xs)))
    return xs, ys

# ── Build legend handles ───────────────────────────────────────────────────────
def make_legend_handles():
    handles = []
    # K line styles
    for k, (ls, lw) in K_STYLES.items():
        handles.append(mlines.Line2D([], [], color="gray", ls=ls, lw=lw, label=f"K={k}"))
    # WA* style
    handles.append(mlines.Line2D([], [], color="gray", ls="--", lw=2.5, label="WA*"))
    # Heuristic colors
    handles.append(mlines.Line2D([], [], color=MS_COLOR,    ls="-", lw=6, alpha=0.4, label="MS"))
    handles.append(mlines.Line2D([], [], color=LMCUT_COLOR, ls="-", lw=6, alpha=0.4, label="LMCut"))
    return handles

style_handles = make_legend_handles()

# ── One figure per weight (time + expansions) ─────────────────────────────────
for wtag, w_label in WEIGHTS:
    # KFS configs for this weight + WA* for this weight
    cfgs_kfs = [f"{wtag}KFS{k}_{h}-ff" for k in KS for h in HEURS]
    cfgs_wa  = [f"{wtag}wa_ms", f"{wtag}wa_lmcut"]
    cfgs     = cfgs_kfs + cfgs_wa

    fig, axes = plt.subplots(1, 2, figsize=(14, 5))
    fig.suptitle(f"Coverage — {w_label}: K-FS vs WA*", fontsize=13, fontweight="bold")

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
                    linewidth=lw, label=label, alpha=0.90)

        ax.set_xlabel(xlabel, fontsize=12)
        ax.set_ylabel("# Solved Problems", fontsize=12)
        ax.set_xscale(xscale)
        if xlim:
            ax.set_xlim(xlim)
        ax.set_ylim(bottom=0)
        ax.grid(True, alpha=0.3, linestyle=":")

        # Per-config legend (lower right)
        h_cfg, l_cfg = [], []
        for cfg in cfgs:
            color, ls, lw, label = style(cfg)
            h_cfg.append(mlines.Line2D([], [], color=color, ls=ls, lw=lw, label=label))
            l_cfg.append(label)
        ax.legend(h_cfg, l_cfg, fontsize=8, loc="lower right", framealpha=0.9, ncol=2)

    # Style guide legend on left subplot (upper left)
    axes[0].add_artist(
        axes[0].legend(style_handles, [h.get_label() for h in style_handles],
                       fontsize=9, loc="upper left", framealpha=0.9,
                       title="Style guide", title_fontsize=8)
    )
    # Re-add per-config legend for axes[0]
    h2, l2 = [], []
    for cfg in cfgs:
        color, ls, lw, label = style(cfg)
        h2.append(mlines.Line2D([], [], color=color, ls=ls, lw=lw, label=label))
        l2.append(label)
    axes[0].legend(h2, l2, fontsize=8, loc="lower right", framealpha=0.9, ncol=2)

    out = COMBINED / f"coverage_KFS_w{wtag}.png"
    fig.tight_layout()
    fig.savefig(out, dpi=150, bbox_inches="tight")
    plt.close(fig)
    print(f"Saved: {out}")

print("\nDone. All outputs in:", COMBINED)
