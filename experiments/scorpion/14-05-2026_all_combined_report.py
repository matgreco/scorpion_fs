#! /usr/bin/env python3

"""
Combined report: WA* + Focal Search + K-Focal Search + Type WA*  (opt-track)
Merges properties from four experiments and generates:
  - AbsoluteReport (all configs)
  - Coverage plots per weight: FS, WA*, KFS (K=10, K=30), Type WA*
    Two panels each: time and expansions
Date: 14-05-2026
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
DATA_DIR   = SCORPION_EXPERIMENTS / "data"
FSWA_EVAL  = DATA_DIR / "05-05-2026_FSvsWA_opt_slurm-eval"
KFS_EVAL   = DATA_DIR / "07-05-2026_KFS_opt_slurm-eval"
TWA_EVAL   = DATA_DIR / "14-05-2026_TypeWA_opt_slurm-eval"
COMBINED   = DATA_DIR / "14-05-2026_all_combined_opt_eval"
COMBINED.mkdir(exist_ok=True)

# ── Load and merge properties ─────────────────────────────────────────────────
def load(path):
    with open(path / "properties") as f:
        return json.load(f)

print("Loading FSvsWA properties (FS + WA*)...")
props_fswa = load(FSWA_EVAL)
print(f"  {len(props_fswa)} runs")

print("Loading KFS properties...")
props_kfs = load(KFS_EVAL)
print(f"  {len(props_kfs)} runs")

print("Loading Type WA* properties...")
props_twa = load(TWA_EVAL)
print(f"  {len(props_twa)} runs")

FS_CONFIGS = {
    "15FS_ms-ff", "15FS_lmcut-ff",
    "20FS_ms-ff", "20FS_lmcut-ff",
    "30FS_ms-ff", "30FS_lmcut-ff",
}
WA_CONFIGS = {
    "15wa_ms", "15wa_lmcut",
    "20wa_ms", "20wa_lmcut",
    "30wa_ms", "30wa_lmcut",
}

KFS_KEEP_K = [10, 30]
KFS_CONFIGS = {
    f"{wtag}KFS{k}_{h}-ff"
    for wtag in ("15", "20", "30")
    for k in KFS_KEEP_K
    for h in ("ms", "lmcut")
}

TWA_CONFIGS = {
    "15typeWA_ms", "15typeWA_lmcut",
    "20typeWA_ms", "20typeWA_lmcut",
    "30typeWA_ms", "30typeWA_lmcut",
}

props_fs  = {k: v for k, v in props_fswa.items() if v.get("algorithm") in FS_CONFIGS}
props_wa  = {k: v for k, v in props_fswa.items() if v.get("algorithm") in WA_CONFIGS}
props_kfs_filt = {k: v for k, v in props_kfs.items() if v.get("algorithm") in KFS_CONFIGS}
props_twa_filt = {k: v for k, v in props_twa.items() if v.get("algorithm") in TWA_CONFIGS}

print(f"  FS runs kept:      {len(props_fs)}")
print(f"  WA* runs kept:     {len(props_wa)}")
print(f"  KFS runs kept:     {len(props_kfs_filt)}")
print(f"  TypeWA* runs kept: {len(props_twa_filt)}")

merged = {**props_fs, **props_wa, **props_kfs_filt, **props_twa_filt}
# Resolve overlaps: TypeWA* > KFS > WA* > FS  (all distinct algorithm sets, no actual conflict)
print(f"  Merged total:      {len(merged)} runs")

with open(COMBINED / "properties", "w") as f:
    json.dump(merged, f)
print(f"Saved merged properties → {COMBINED / 'properties'}")

# ── AbsoluteReport ────────────────────────────────────────────────────────────
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
    outfile = COMBINED / "report-all-combined-opt.html"
    report(str(COMBINED), str(outfile))
    print(f"Saved HTML report → {outfile}")
except Exception as e:
    print(f"Lab report failed ({e}). Skipping HTML report.")

# ── Collect coverage data ─────────────────────────────────────────────────────
ALL_CONFIGS = FS_CONFIGS | WA_CONFIGS | KFS_CONFIGS | TWA_CONFIGS

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
for c in sorted(ALL_CONFIGS):
    print(f"  {c}: {len(solved_times[c])}")

# ── Style ──────────────────────────────────────────────────────────────────────
MS_COLOR    = "#2a77cc"
LMCUT_COLOR = "#d06000"

def style(cfg):
    is_ms = "ms" in cfg and "lmcut" not in cfg
    color = MS_COLOR if is_ms else LMCUT_COLOR
    h_str = "MS" if is_ms else "LMCut"

    if "typeWA" in cfg:
        return color, (0, (5, 1)), 2.2, f"TypeWA*({h_str})"
    if cfg.startswith(("15wa", "20wa", "30wa")):
        return color, "-", 2.5, f"WA*({h_str})"
    if cfg.startswith(("15FS", "20FS", "30FS")):
        return color, "--", 2.0, f"FS({h_str})"
    # KFS
    for k in KFS_KEEP_K:
        if f"KFS{k}_" in cfg:
            ls = ":" if k == 10 else (0, (3, 1, 1, 1))
            lw = 1.8 if k == 10 else 1.6
            return color, ls, lw, f"KFS-{k}({h_str})"
    return color, "-", 1.5, cfg

def coverage_curve(sorted_vals):
    xs = [0.0] + sorted_vals
    ys = list(range(len(xs)))
    return xs, ys

# ── Legend handles ─────────────────────────────────────────────────────────────
def make_style_handles():
    handles = []
    handles.append(mlines.Line2D([], [], color="gray", ls="-",          lw=2.5, label="WA*"))
    handles.append(mlines.Line2D([], [], color="gray", ls="--",         lw=2.0, label="FS"))
    handles.append(mlines.Line2D([], [], color="gray", ls=(0,(5,1)),    lw=2.2, label="TypeWA*"))
    handles.append(mlines.Line2D([], [], color="gray", ls=":",          lw=1.8, label="KFS K=10"))
    handles.append(mlines.Line2D([], [], color="gray", ls=(0,(3,1,1,1)),lw=1.6, label="KFS K=30"))
    handles.append(mlines.Line2D([], [], color=MS_COLOR,    ls="-", lw=6, alpha=0.4, label="MS"))
    handles.append(mlines.Line2D([], [], color=LMCUT_COLOR, ls="-", lw=6, alpha=0.4, label="LMCut"))
    return handles

style_handles = make_style_handles()

# ── One figure per weight ──────────────────────────────────────────────────────
WEIGHTS = [("15", "w=1.5"), ("20", "w=2.0"), ("30", "w=3.0")]

for wtag, w_label in WEIGHTS:
    cfgs = (
        [f"{wtag}FS_{h}-ff"      for h in ("ms", "lmcut")] +
        [f"{wtag}wa_{h}"         for h in ("ms", "lmcut")] +
        [f"{wtag}typeWA_{h}"     for h in ("ms", "lmcut")] +
        [f"{wtag}KFS{k}_{h}-ff"  for k in KFS_KEEP_K for h in ("ms", "lmcut")]
    )

    fig, axes = plt.subplots(1, 2, figsize=(14, 5))
    fig.suptitle(f"Coverage — {w_label}: WA* vs FS vs KFS vs TypeWA*", fontsize=13, fontweight="bold")

    for ax, (data_dict, xlabel, xscale, xlim) in zip(axes, [
        (solved_times,  "Time (s)",   "linear", (0, 620)),
        (solved_expans, "Expansions", "log",     None),
    ]):
        for cfg in cfgs:
            vals = data_dict.get(cfg, [])
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

        h_cfg, l_cfg = [], []
        for cfg in cfgs:
            color, ls, lw, label = style(cfg)
            h_cfg.append(mlines.Line2D([], [], color=color, ls=ls, lw=lw, label=label))
            l_cfg.append(label)
        ax.legend(h_cfg, l_cfg, fontsize=8, loc="lower right", framealpha=0.9, ncol=2)

    axes[0].add_artist(
        axes[0].legend(style_handles, [h.get_label() for h in style_handles],
                       fontsize=9, loc="upper left", framealpha=0.9,
                       title="Style guide", title_fontsize=8)
    )
    h2, l2 = [], []
    for cfg in cfgs:
        color, ls, lw, label = style(cfg)
        h2.append(mlines.Line2D([], [], color=color, ls=ls, lw=lw, label=label))
        l2.append(label)
    axes[0].legend(h2, l2, fontsize=8, loc="lower right", framealpha=0.9, ncol=2)

    out = COMBINED / f"coverage_all_w{wtag}.png"
    fig.tight_layout()
    fig.savefig(out, dpi=150, bbox_inches="tight")
    plt.close(fig)
    print(f"Saved: {out}")

print("\nDone. All outputs in:", COMBINED)
