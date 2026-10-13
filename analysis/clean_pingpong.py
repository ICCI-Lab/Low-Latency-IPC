#!/usr/bin/env python3
import csv
from pathlib import Path

def load_latencies(csv_path: Path):
    vals = []
    with csv_path.open() as f:
        r = csv.DictReader(f)
        for row in r:
            try:
                vals.append(int(row["lat_ns"]))
            except Exception:
                continue
    if not vals:
        raise SystemExit("no lat_ns data found")
    return vals

def quantile(sorted_vals, q: float):
    n = len(sorted_vals)
    if n == 0:
        return 0
    # ساده و قابل دفاع
    idx = int(round((n - 1) * q))
    return sorted_vals[idx]

def summarize(name: str, data, out):
    s = sorted(data)
    n = len(s)
    avg = sum(s) / n
    p50 = quantile(s, 0.50)
    p95 = quantile(s, 0.95)
    p99 = quantile(s, 0.99)
    line = (
        f"{name}: n={n}, "
        f"avg={avg:.1f} ns, "
        f"p50={p50} ns, "
        f"p95={p95} ns, "
        f"p99={p99} ns"
    )
    print(line)
    out.write(line + "\n")

def main():
    csv_path = Path("internal_fastpath_pingpong.csv")
    if not csv_path.exists():
        raise SystemExit(f"{csv_path} not found")

    vals = load_latencies(csv_path)
    N = len(vals)
    s = sorted(vals)

    summary_path = Path("internal_fastpath_pingpong_summary.txt")
    with summary_path.open("w") as out:
        out.write("# internal fastpath ping-pong latency summary\n")
        out.write(f"# N_raw = {N}\n\n")

        # خام
        summarize("RAW", s, out)

        # trim دو سر برای حذف outlier و warm-up
        k = max(1, int(N * 0.005))   # 0.5%
        trimmed = s[k:-k] if 2 * k < N else s
        summarize(f"CLEAN_trim_0.5pct_each_side (k={k})", trimmed, out)

    print(f"summary written to {summary_path}")

if __name__ == "__main__":
    main()
