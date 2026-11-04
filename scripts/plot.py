#!/usr/bin/env python3
"""Usage: python3 scripts/plot.py results/results.csv results/   -> writes PNG figures."""
import sys, os
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

csv = sys.argv[1] if len(sys.argv) > 1 else "results/results.csv"
outdir = sys.argv[2] if len(sys.argv) > 2 else "results"
os.makedirs(outdir, exist_ok=True)
df = pd.read_csv(csv)
STYLE = {"standard": dict(color="tab:red", marker="o"), "conservative": dict(color="tab:blue", marker="s")}

def agg(sub, x, y):
    """mean and std over trials, per (variant, x)."""
    g = sub.groupby(["variant", x])[y].agg(["mean", "std"]).reset_index()
    return {v: gg.sort_values(x) for v, gg in g.groupby("variant")}

def line(ax, sub, x, y, **kw):
    for v, g in agg(sub, x, y).items():
        ax.errorbar(g[x], g["mean"], yerr=g["std"].fillna(0), label=v, capsize=3, **STYLE[v], **kw)

def save(fig, name):
    fig.tight_layout(); fig.savefig(os.path.join(outdir, name), dpi=160); plt.close(fig)
    print("wrote", name)

# 1. Error vs width (log-log) with theoretical eps*N bound
w = df[df.experiment == "width"]
fig, ax = plt.subplots(1, 2, figsize=(11, 4))
line(ax[0], w, "w", "mean_abs_err")
b = w.groupby("w")["eps_N"].mean()
ax[0].plot(b.index, b.values, "k--", label=r"bound $eN/w$")
ax[0].set(xscale="log", yscale="log", xlabel="width w (d=4)", ylabel="mean abs. error (all items)",
          title="Overestimation vs. sketch width"); ax[0].legend(); ax[0].grid(alpha=.3)
line(ax[1], w, "w", "mean_rel_err_topk")
ax[1].set(xscale="log", yscale="log", xlabel="width w (d=4)", ylabel="mean relative error (top-100)",
          title="Error on the heaviest items"); ax[1].legend(); ax[1].grid(alpha=.3)
save(fig, "fig1_error_vs_width.png")

# 2. Memory vs accuracy trade-off (bytes), incl. exact baseline
fig, ax = plt.subplots(figsize=(6, 4))
for v, g in agg(w, "mem_bytes", "mean_abs_err").items():
    ax.plot(g["mem_bytes"] / 1024, g["mean"], label=v, **STYLE[v])
ex = w["exact_mem_bytes"].mean() / 1024
ax.axvline(ex, color="gray", ls=":", label=f"exact hash map (~{ex/1024:.0f} MB)")
ax.set(xscale="log", yscale="log", xlabel="memory (KB)", ylabel="mean abs. error",
       title="Memory vs. accuracy"); ax.legend(); ax.grid(alpha=.3)
save(fig, "fig2_memory_vs_error.png")

# 3. Heavy-hitter precision vs width (recall is 1 by construction)
fig, ax = plt.subplots(figsize=(6, 4))
line(ax, w, "w", "hh_precision")
ax.set(xscale="log", xlabel="width w (d=4)", ylabel="precision (phi = 0.001)",
       title="Heavy-hitter detection precision"); ax.legend(); ax.grid(alpha=.3)
save(fig, "fig3_heavy_hitter_precision.png")

# 4. Distribution comparison
d = df[df.experiment == "dist"].copy()
d["label"] = d.apply(lambda r: r.dist if r.dist != "zipf" else f"zipf {r.param}", axis=1)
order = ["uniform", "zipf 0.6", "zipf 0.8", "zipf 1.0", "zipf 1.2", "zipf 1.5", "flows"]
fig, ax = plt.subplots(1, 2, figsize=(12, 4))
for j, (col, ttl) in enumerate([("mean_abs_err", "Mean abs. error"), ("hh_precision", "HH precision")]):
    p = d.groupby(["label", "variant"])[col].mean().unstack().reindex(order)
    p.plot.bar(ax=ax[j], color=[STYLE[c]["color"] for c in p.columns], rot=30)
    ax[j].set(title=f"{ttl} by distribution (w=4096, d=4)", xlabel="")
    ax[j].grid(alpha=.3, axis="y")
ax[0].set_yscale("log")
save(fig, "fig4_distributions.png")

# 5. Depth: fixed memory budget (accuracy) and fixed width (throughput)
fm = df[df.experiment == "depth_fixed_mem"]; fw = df[df.experiment == "depth_fixed_w"]
fig, ax = plt.subplots(1, 2, figsize=(11, 4))
line(ax[0], fm, "d", "mean_abs_err")
ax[0].set(xlabel="depth d (w*d = 16384 fixed)", ylabel="mean abs. error", yscale="log",
          title="Depth vs. width at equal memory"); ax[0].legend(); ax[0].grid(alpha=.3)
line(ax[1], fw, "d", "update_mups")
ax[1].set(xlabel="depth d (w=4096)", ylabel="update throughput (M updates/s)",
          title="Throughput vs. depth"); ax[1].legend(); ax[1].grid(alpha=.3)
ax[1].axhline(fw["exact_mups"].mean(), color="gray", ls=":", label="exact hash map"); ax[1].legend()
save(fig, "fig5_depth.png")

# 6. Stream length
s = df[df.experiment == "streamlen"]
fig, ax = plt.subplots(1, 2, figsize=(11, 4))
line(ax[0], s, "N", "mean_abs_err")
ax[0].set(xscale="log", yscale="log", xlabel="stream length N", ylabel="mean abs. error",
          title="Error grows linearly with N (w=4096, d=4)"); ax[0].legend(); ax[0].grid(alpha=.3)
line(ax[1], s, "N", "update_mups")
ax[1].set(xscale="log", xlabel="stream length N", ylabel="M updates/s", title="Throughput vs. stream length")
ax[1].legend(); ax[1].grid(alpha=.3)
save(fig, "fig6_stream_length.png")

# Summary table
summ = (w.groupby(["w", "variant"])[["mem_bytes", "mean_abs_err", "max_err", "frac_exceed", "hh_precision", "update_mups"]]
        .mean().round(4))
summ.to_csv(os.path.join(outdir, "summary_width.csv")); print(summ.to_string())
