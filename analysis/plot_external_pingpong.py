import os
import math
import csv
from typing import List, Dict
import matplotlib.pyplot as plt

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
DATA_DIR = os.path.join(BASE_DIR, "data_external")
PLOTS_DIR = os.path.join(BASE_DIR, "..", "Plots")

os.makedirs(PLOTS_DIR, exist_ok=True)

METHODS = [
    ("udp", "UDP"),
    ("tcp", "TCP"),
    ("io_uring_tcp", "io_uring TCP"),
]

MSG_SIZES = [64, 512, 4096, 65536]  # bytes


def load_lat_ns(path: str) -> List[int]:
    vals: List[int] = []
    with open(path, "r") as f:
        reader = csv.reader(f)
        first = True
        for row in reader:
            if not row:
                continue
            if first:
                first = False
                # اگر هدر باشد، رد کن
                try:
                    int(row[0])
                except ValueError:
                    continue
            # فرمت: seq,lat_ns
            try:
                lat_ns = int(row[1])
            except (IndexError, ValueError):
                continue
            vals.append(lat_ns)
    if not vals:
        raise ValueError(f"no data in {path}")
    return vals


def percentiles(sorted_vals: List[int]):
    n = len(sorted_vals)
    if n == 1:
        v = sorted_vals[0]
        return v, v, v

    def p(q: float) -> int:
        pos = q * (n - 1) / 100.0
        i = int(pos)
        j = min(i + 1, n - 1)
        frac = pos - i
        return int((1 - frac) * sorted_vals[i] + frac * sorted_vals[j])

    return p(50), p(95), p(99)


def summarize_all() -> Dict[str, Dict[int, Dict[str, float]]]:
    """
    result[method][msg_size] = {"p50":..,"p95":..,"p99":..,"avg":..} در ns
    """
    result: Dict[str, Dict[int, Dict[str, float]]] = {}
    for method, _label in METHODS:
        result[method] = {}
        for size in MSG_SIZES:
            fname = f"{method}_{size}B.csv"
            path = os.path.join(DATA_DIR, fname)
            if not os.path.exists(path):
                # مثلا udp_65536B.csv نداريم
                continue
            vals = load_lat_ns(path)
            vals_sorted = sorted(vals)
            p50, p95, p99 = percentiles(vals_sorted)
            avg = sum(vals_sorted) / len(vals_sorted)
            result[method][size] = {
                "p50": p50,
                "p95": p95,
                "p99": p99,
                "avg": avg,
            }
            print(f"{fname}: n={len(vals_sorted)}, avg={avg:.1f} ns, "
                  f"p50={p50} ns, p95={p95} ns, p99={p99} ns")
    return result


def plot_metric(stats, metric: str, filename: str, title: str):
    plt.figure()
    for method, label in METHODS:
        sizes = []
        vals_us = []
        for size in MSG_SIZES:
            if size not in stats[method]:
                continue
            sizes.append(size)
            vals_us.append(stats[method][size][metric] / 1000.0)  # ns -> µs
        if not sizes:
            continue
        plt.plot(sizes, vals_us, marker="o", label=label)

    plt.xlabel("Message size (bytes)")
    plt.ylabel(f"{metric.upper()} RTT (µs)")
    plt.xscale("log", base=2)
    plt.grid(True, which="both", linestyle="--", linewidth=0.5)
    plt.legend()
    plt.tight_layout()
    out_path = os.path.join(PLOTS_DIR, filename)
    plt.savefig(out_path, dpi=300)
    plt.close()
    print(f"[PLOT] saved: {out_path}")


def main():
    stats = summarize_all()
    plot_metric(stats, "p50", "external_latency_p50.png",
                "External ping-pong P50")
    plot_metric(stats, "p95", "external_latency_p95.png",
                "External ping-pong P95")
    plot_metric(stats, "p99", "external_latency_p99.png",
                "External ping-pong P99")
    plot_metric(stats, "avg", "external_latency_avg.png",
                "External ping-pong AVG")


if __name__ == "__main__":
    main()
