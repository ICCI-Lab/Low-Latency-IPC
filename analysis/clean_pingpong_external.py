import sys, os

def load_ns(path: str):
    vals = []
    with open(path, "r") as f:
        first = True
        for line in f:
            line = line.strip()
            if not line:
                continue

            # اگر سطر اول هدر بود، ردش کن
            if first:
                first = False
                c0 = line[0]
                if not (c0.isdigit() or c0 in "+-"):
                    # هدر متنی
                    continue

            # CSV: ممکن است "index,ns" باشد
            parts = [p.strip() for p in line.split(",") if p.strip()]
            if not parts:
                continue
            v = parts[-1]

            # فقط رقم را نگه دار
            if not (v[0].isdigit() or v[0] in "+-"):
                continue
            try:
                vals.append(int(v))
            except ValueError:
                # اگر باز هم غیرعددی بود، رد کن
                continue

    if not vals:
        raise ValueError(f"no numeric data in {path}")
    return vals

def percentiles(sorted_vals):
    n = len(sorted_vals)
    if n == 1:
        v = sorted_vals[0]
        return v, v, v

    def p(q):
        pos = q * (n - 1) / 100.0
        i = int(pos)
        j = min(i + 1, n - 1)
        frac = pos - i
        return int((1 - frac) * sorted_vals[i] + frac * sorted_vals[j])

    return p(50), p(95), p(99)

def summarize(path: str):
    vals = load_ns(path)
    n = len(vals)
    s = sorted(vals)

    p50, p95, p99 = percentiles(s)
    avg = sum(vals) / n

    # trim کردن ۰٫۵٪ از هر طرف
    k = int(n * 0.005)
    if 2 * k < n:
        s_trim = s[k:n - k]
    else:
        s_trim = s
        k = 0

    n2 = len(s_trim)
    avg2 = sum(s_trim) / n2
    p50_2, p95_2, p99_2 = percentiles(s_trim)

    base = os.path.basename(path)
    name_no_ext, _ = os.path.splitext(base)
    out_path = os.path.join(os.path.dirname(path),
                            f"{name_no_ext}_summary.txt")

    with open(out_path, "w") as out:
        out.write(f"file={base}\n")
        out.write(
            f"RAW: n={n}, avg={avg:.1f} ns, "
            f"p50={p50} ns, p95={p95} ns, p99={p99} ns\n"
        )
        out.write(
            f"CLEAN_trim_0.5pct_each_side (k={k}): "
            f"n={n2}, avg={avg2:.1f} ns, "
            f"p50={p50_2} ns, p95={p95_2} ns, p99={p99_2} ns\n"
        )

    print(f"{base}:")
    print(f"RAW: n={n}, avg={avg:.1f} ns, p50={p50} ns, p95={p95} ns, p99={p99} ns")
    print(
        f"CLEAN_trim_0.5pct_each_side (k={k}): "
        f"n={n2}, avg={avg2:.1f} ns, p50={p50_2} ns, "
        f"p95={p95_2} ns, p99={p99_2} ns"
    )
    print(f"summary written to {out_path}")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <csv_path>", file=sys.stderr)
        sys.exit(1)
    summarize(sys.argv[1])
