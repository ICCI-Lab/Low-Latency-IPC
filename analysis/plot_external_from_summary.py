import os, re
import matplotlib.pyplot as plt

BASE = os.path.join(os.path.dirname(__file__), "data_external")
PROP = os.path.join(BASE, "Data_Proposed_External")

# (label, {size_bytes: summary_path})
METHODS = [
    ("TCP", {
        64:    os.path.join(BASE, "tcp_64B_summary.txt"),
        512:   os.path.join(BASE, "tcp_512B_summary.txt"),
        4096:  os.path.join(BASE, "tcp_4096B_summary.txt"),
        65536: os.path.join(BASE, "tcp_65536B_summary.txt"),
    }),
    ("UDP", {
        64:    os.path.join(BASE, "udp_64B_summary.txt"),
        512:   os.path.join(BASE, "udp_512B_summary.txt"),
        4096:  os.path.join(BASE, "udp_4096B_summary.txt"),
        65536: os.path.join(BASE, "udp_65000B_summary.txt"),  # طبق اسکرين شات تو 65KB اين اسم را داري
    }),
    ("io_uring (baseline)", {
        64:    os.path.join(BASE, "io_uring_tcp_64B_summary.txt"),
        512:   os.path.join(BASE, "io_uring_tcp_512B_summary.txt"),
        4096:  os.path.join(BASE, "io_uring_tcp_4096B_summary.txt"),
        65536: os.path.join(BASE, "io_uring_tcp_65536B_summary.txt"),
    }),
    ("Proposed (External)", {
        64:    os.path.join(PROP, "fast64_64B_summary.txt"),
        512:   os.path.join(PROP, "fast512_512B_summary.txt"),
        4096:  os.path.join(PROP, "final_4096B_summary.txt"),
        65536: os.path.join(PROP, "io_uring_tcp_65536B_fragile_summary.txt"),
    }),
]

# parse CLEAN line: avg/p50/p95/p99 in ns
CLEAN_RE = re.compile(r"CLEAN.*avg=(?P<avg>[\d.]+)\s*ns.*p50=(?P<p50>\d+)\s*ns.*p95=(?P<p95>\d+)\s*ns.*p99=(?P<p99>\d+)\s*ns")

def parse_summary(path: str):
    with open(path, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            m = CLEAN_RE.search(line)
            if m:
                d = m.groupdict()
                return {
                    "avg": float(d["avg"]),
                    "p50": float(d["p50"]),
                    "p95": float(d["p95"]),
                    "p99": float(d["p99"]),
                }
    raise ValueError(f"No CLEAN line parsed in: {path}")

def ensure_files():
    missing = []
    for _, mp in METHODS:
        for sz, p in mp.items():
            if not os.path.exists(p):
                missing.append(p)
    return missing

missing = ensure_files()
if missing:
    print("MISSING summary files:")
    for p in missing:
        print(" -", p)
    raise SystemExit(2)

sizes = [64, 512, 4096, 65536]
metrics = ["p50", "p95", "p99", "avg"]

out_dir = BASE
os.makedirs(out_dir, exist_ok=True)

for metric in metrics:
    plt.figure()
    for label, mp in METHODS:
        xs = []
        ys = []
        for sz in sizes:
            st = parse_summary(mp[sz])
            xs.append(sz)
            ys.append(st[metric])
        plt.plot(xs, ys, marker="o", label=label)

    plt.xscale("log")
    plt.yscale("log")
    plt.xlabel("Message size (bytes)")
    ylabel = "Average latency (ns)" if metric == "avg" else f"{metric} latency (ns)"
    plt.ylabel(ylabel)
    plt.title(f"external_{metric}")
    plt.legend()
    plt.tight_layout()

    out_path = os.path.join(out_dir, f"external_latency_{metric}.png")
    plt.savefig(out_path, dpi=200)
    print("WROTE:", out_path)

print("DONE")
