#!/usr/bin/env python3
from __future__ import annotations

import re
from pathlib import Path
import matplotlib.pyplot as plt

SIZES = [64, 512, 4096, 65536]  # محور X

# براي هر روش، از summaryها (ترجيحا CLEAN) ميخوانيم.
# اگر چند فايل match شود، جديدترين (mtime) انتخاب ميشود.
METHODS = [
    ("TCP", [
        "tcp_64B_summary.txt",
        "tcp_512B_summary.txt",
        "tcp_4096B_summary.txt",
        "tcp_65536B_summary.txt",
    ]),
    ("UDP", [
        "udp_64B_summary.txt",
        "udp_512B_summary.txt",
        "udp_4096B_summary.txt",
        # بعضي جاها 65000 نوشتي؛ همين را معادل 65536 رسم ميكنيم
        "udp_65536B_summary.txt",
        "udp_65000B_summary.txt",
    ]),
    ("io_uring (baseline)", [
        "io_uring_tcp_64B_summary.txt",
        "io_uring_tcp_512B_summary.txt",
        "io_uring_tcp_4096B_summary.txt",
        "io_uring_tcp_65536B_summary.txt",
    ]),
    ("Proposed (External)", [
        # داخل Data_Proposed_External دنبال summary ميگرديم
        "Data_Proposed_External/*64*_summary.txt",
        "Data_Proposed_External/*512*_summary.txt",
        "Data_Proposed_External/*4096*_summary.txt",
        "Data_Proposed_External/*65536*_summary.txt",
    ]),
]

CLEAN_RE = re.compile(
    r"CLEAN.*avg=(?P<avg>[\d.]+)\s*ns,\s*p50=(?P<p50>[\d.]+)\s*ns,\s*p95=(?P<p95>[\d.]+)\s*ns,\s*p99=(?P<p99>[\d.]+)\s*ns",
    re.IGNORECASE,
)
RAW_RE = re.compile(
    r"RAW.*avg=(?P<avg>[\d.]+)\s*ns,\s*p50=(?P<p50>[\d.]+)\s*ns,\s*p95=(?P<p95>[\d.]+)\s*ns,\s*p99=(?P<p99>[\d.]+)\s*ns",
    re.IGNORECASE,
)

def newest_match(base: Path, patterns: list[str]) -> Path | None:
    cand: list[Path] = []
    for pat in patterns:
        cand += list(base.glob(pat))
    cand = [p for p in cand if p.is_file()]
    if not cand:
        return None
    return max(cand, key=lambda p: p.stat().st_mtime)

def parse_summary(p: Path) -> dict[str, float] | None:
    txt = p.read_text(errors="ignore")
    m = CLEAN_RE.search(txt)
    if not m:
        m = RAW_RE.search(txt)
    if not m:
        return None
    return {k: float(m.group(k)) for k in ("avg", "p50", "p95", "p99")}

def main():
    base = Path(".").resolve()

    series = {name: {k: [] for k in ("avg","p50","p95","p99")} for (name, _) in METHODS}
    used_files: dict[str, list[str]] = {name: [] for (name, _) in METHODS}

    for name, pats in METHODS:
        # pats براي هر سايز بايد يك pattern (يا چندتا) بدهد.
        # اگر UDP دو تا pattern براي نقطه آخر داشت، هندل ميكنيم.
        per_size_patterns: list[list[str]] = []
        if name == "UDP":
            per_size_patterns = [
                [pats[0]],
                [pats[1]],
                [pats[2]],
                [pats[3], pats[4]],
            ]
        else:
            per_size_patterns = [[p] for p in pats]

        for i, spats in enumerate(per_size_patterns):
            p = newest_match(base, spats)
            if p is None:
                # اگر پيدا نشد، NaN ميگذاريم تا خط قطع شود
                for k in ("avg","p50","p95","p99"):
                    series[name][k].append(float("nan"))
                used_files[name].append("MISSING")
                continue

            d = parse_summary(p)
            if d is None:
                for k in ("avg","p50","p95","p99"):
                    series[name][k].append(float("nan"))
                used_files[name].append(str(p))
                continue

            for k in ("avg","p50","p95","p99"):
                series[name][k].append(d[k])
            used_files[name].append(str(p))

    print("== Using summary files ==")
    for name, files in used_files.items():
        print(f"\n[{name}]")
        for sz, f in zip(SIZES, files):
            print(f"  {sz:>5}B -> {f}")

    # ---- Plot: 2x2 (avg,p50,p95,p99) ----
    metrics = [("avg","external_avg"), ("p50","external_p50"), ("p95","external_p95"), ("p99","external_p99")]
    fig, axes = plt.subplots(2, 2, figsize=(12, 8), constrained_layout=True)
    axes = axes.ravel()

    for ax, (mkey, title) in zip(axes, metrics):
        for name, _ in METHODS:
            ax.plot(SIZES, series[name][mkey], marker="o", label=name)
        ax.set_xscale("log")
        ax.set_yscale("log")
        ax.set_title(title)
        ax.set_xlabel("Message size (bytes)")
        ax.set_ylabel(f"{mkey} latency (ns)")
        ax.grid(True, which="both", linestyle="--", alpha=0.3)
        ax.legend()

    out_all = base / "external_latency_all.png"
    fig.savefig(out_all, dpi=200)

    # ---- Save individual plots too ----
    for (mkey, title) in metrics:
        plt.figure(figsize=(8,5))
        for name, _ in METHODS:
            plt.plot(SIZES, series[name][mkey], marker="o", label=name)
        plt.xscale("log")
        plt.yscale("log")
        plt.title(title)
        plt.xlabel("Message size (bytes)")
        plt.ylabel(f"{mkey} latency (ns)")
        plt.grid(True, which="both", linestyle="--", alpha=0.3)
        plt.legend()
        plt.savefig(base / f"{title}.png", dpi=200, bbox_inches="tight")
        plt.close()

    print(f"\nSaved: {out_all}")
    print("Also saved: external_avg.png, external_p50.png, external_p95.png, external_p99.png")

if __name__ == "__main__":
    main()
