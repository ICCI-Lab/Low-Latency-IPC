#!/usr/bin/env python3

import csv
import re
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

BASE = Path(__file__).resolve().parent
SIZES = [64, 512, 4096, 65536]
X_LABELS = ["64B", "512B", "4KB", "64KB"]

plt.rcParams.update({
    "font.family": "DejaVu Sans",
    "font.size": 13,
    "axes.titlesize": 17,
    "axes.titleweight": "bold",
    "axes.labelsize": 14,
    "axes.labelweight": "medium",
    "xtick.labelsize": 12,
    "ytick.labelsize": 12,
    "legend.fontsize": 12,
    "figure.titlesize": 24,
    "figure.titleweight": "bold",
    "lines.linewidth": 2.3,
    "lines.markersize": 8,
    "savefig.dpi": 600,
    "axes.unicode_minus": False,
})

def read_csv_stats(path: Path):
    vals = []
    with path.open(newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            try:
                vals.append(int(row["lat_ns"]))
            except (KeyError, ValueError, TypeError):
                continue

    vals.sort()
    n = len(vals)

    return {
        "avg": sum(vals) / n,
        "p50": vals[int(n * 0.50)],
        "p95": vals[int(n * 0.95)],
        "p99": vals[int(n * 0.99)],
    }

def read_io_uring_stats(path: Path):
    text = path.read_text(errors="ignore")
    m = re.search(
        r"p50=(\d+)\s+ns,\s*"
        r"p95=(\d+)\s+ns,\s*"
        r"p99=(\d+)\s+ns,\s*"
        r"avg=([\d.]+)\s+ns",
        text,
    )
    return {
        "p50": float(m.group(1)),
        "p95": float(m.group(2)),
        "p99": float(m.group(3)),
        "avg": float(m.group(4)),
    }

METHODS = {
    "UDS": {},
    "Pipe": {},
    "io_uring": {},
    "Proposed": {},
}

for size in SIZES:
    METHODS["UDS"][size] = read_csv_stats(BASE / f"uds_{size}B.csv")
    METHODS["Pipe"][size] = read_csv_stats(BASE / f"pipe_{size}B.csv")
    METHODS["io_uring"][size] = read_io_uring_stats(BASE / f"io_uring_internal_{size}B.txt")
    METHODS["Proposed"][size] = read_csv_stats(BASE / f"pingpong_{size}B.csv")

TITLES = {
    "p50": "P50 Latency",
    "p95": "P95 Latency",
    "avg": "Average Latency",
    "p99": "P99 Latency",
}

def draw_metric(ax, metric):
    for method, values in METHODS.items():
        ys = [values[size][metric] for size in SIZES]
        ax.plot(SIZES, ys, marker="o", label=method)

    ax.set_xscale("log", base=2)
    ax.set_yscale("log")
    ax.set_xticks(SIZES)
    ax.set_xticklabels(X_LABELS, fontweight="medium")
    ax.set_xlabel("Message Size (Bytes)", labelpad=8)
    ax.set_ylabel("Latency (ns)", labelpad=8)
    ax.set_title(TITLES[metric], pad=10)
    ax.grid(True, which="both", linestyle="--", linewidth=0.5, alpha=0.25)
    ax.legend(frameon=True, fancybox=True, framealpha=0.95)

fig, axes = plt.subplots(2, 2, figsize=(18, 11))
metrics = ["p50", "p95", "avg", "p99"]

for ax, metric in zip(axes.ravel(), metrics):
    draw_metric(ax, metric)

fig.suptitle("Internal IPC", y=0.985)
fig.tight_layout(rect=[0.02, 0.02, 0.98, 0.955], h_pad=2.2, w_pad=2.2)

png_out = BASE / "internal_ipc_all_en_hq.png"
fig.savefig(
    png_out,
    dpi=600,
    bbox_inches="tight",
    facecolor="white",
)

plt.close(fig)
print(f"[PNG] {png_out}")
