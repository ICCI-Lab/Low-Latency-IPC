# Data Processing

External CSV files use latency values in nanoseconds, principally `lat_ns`.

## Cleaning
```bash
python3 clean_pingpong_external.py FILE.csv
```

Historical processing reports:
```text
CLEAN_trim_0.5pct_each_side
```
This removes 0.5% of the lowest and 0.5% of the highest samples. For 20000 raw samples, the cleaned set is typically 19800 samples.

Metrics:
- AVG
- P50
- P95
- P99

## Proposed External Historical Names
```text
fast64_64B.csv
fast512_512B.csv
final_4096B.csv
io_uring_tcp_65536B_fragile.csv
```

## Internal Saved-Data Workflow
Existing results may be analyzed without rerunning benchmarks:
```text
READ EXISTING DATA ONLY
NO BUILD
NO BENCHMARK
NO FILE WRITE
```

Historical plotting commands include:
```bash
python3 plot_internal_pingpong_all_en_hq.py
python3 plot_external_pingpong_all.py
python3 plot_external_from_summary.py
```
