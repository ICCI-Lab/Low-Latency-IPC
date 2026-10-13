# Study Guide

This guide defines the recommended order for studying the implementation.

It focuses on architecture, source-code reasoning, methodology, and thesis
defense preparation.

Numerical benchmark results are intentionally omitted.

# 1. First Understand the Research Structure

```text
Low-Latency IPC
|
+-- Internal IPC
|   |
|   +-- UDS baseline
|   +-- Pipe baseline
|   +-- io_uring baseline
|   +-- Proposed Internal Fast Path
|
+-- External IPC
    |
    +-- TCP baseline
    +-- UDP baseline
    +-- io_uring TCP baseline
    +-- Proposed External
```

The baselines answer:

```text
How do conventional or reference communication paths behave?
```

The proposed methods answer:

```text
Which software, synchronization, scheduling, memory, or I/O overheads can be
reduced, and what trade-offs are introduced?
```

# 2. Internal Study Order

Study:

```text
uds_pingpong.c
        |
pipe_pingpong.c
        |
io_uring_pingpong.c
        |
InternalFastPath/
```

For each baseline, understand:
- setup
- data path
- system calls
- blocking/waiting behavior
- synchronization
- latency measurement

Then study the proposed internal path:
- `shm_rings.h`
- producer side
- consumer side
- ping-pong versions
- parameterized benchmark
- build script

The goal is to understand why shared memory plus lock-free coordination can
avoid some repeated kernel-mediated IPC work.

# 3. External Baseline Study Order

Study:

```text
TCP
 |
UDP
 |
io_uring TCP
```

For each one, trace:

```text
VM_1 client
   |
Linux networking
   |
virtual NIC
   |
VMware host-only network
   |
virtual NIC
   |
Linux networking
   |
VM_2 server
```

Understand:
- connection/setup model
- send/receive path
- reliability semantics
- message boundaries
- socket options
- io_uring submission/completion behavior
- where the kernel still participates

# 4. Proposed External Must Be Studied Separately

Do not treat `Proposed_External` as merely “another io_uring benchmark”.

It is a family of experimental variants.

Client files include:
- fixed path
- fast variants
- payload-specific variants
- final variant
- IRQ variants
- KPI variant

Server files include:
- echo/fixed paths
- fast/final variants
- large-message-specific variant

The exact optimization set differs by file.

# 5. Proposed External Reading Order

A useful reading order is:

```text
1. ext_tcp_client_fixed.c
2. ext_tcp_server_fixed.c

3. io_uring_tcp_client_fast64.c
4. io_uring_tcp_client_fast512.c
5. io_uring_tcp_client_fast.c
6. io_uring_tcp_client_final.c

7. io_uring_tcp_server_fast.c
8. io_uring_tcp_server_final.c
9. io_uring_tcp_server_fixed65536.c

10. io_uring_tcp_client_irq.c
11. io_uring_tcp_client_irq_v2.c
12. io_uring_tcp_client_irq_final.c

13. io_uring_tcp_client_kpi.c
```

For every step compare it with the previous implementation.

Ask:
- what changed?
- why was it changed?
- which overhead was targeted?
- what new trade-off appeared?
- was the variant intended for final measurement or investigation?

# 6. Concepts to Master for Proposed External

## io_uring

Understand:
- SQ
- CQ
- SQE
- CQE
- submission
- completion
- blocking vs asynchronous behavior

## Fixed / Registered Buffers

Understand why pre-registering memory can reduce repeated per-operation work.

## Registered Files

Understand why registering descriptors can reduce repeated lookup/setup work.

## CPU Affinity

Understand:
- CPU pinning
- scheduler migration
- cache locality
- benchmark jitter

## Polling

Understand:
- busy polling
- wakeup reduction
- interrupt/scheduler latency
- CPU-consumption trade-off

## Socket Tuning

Understand the role of options found in the source, such as:
- TCP_NODELAY
- TCP_QUICKACK
- socket buffers
- SO_BUSY_POLL

Do not attribute any option to a variant until it is confirmed in that source.

# 7. Measurement Methodology

Understand the ping-pong model:

```text
client sends
     |
server receives
     |
server replies
     |
client receives
     |
RTT recorded
```

Understand the difference between:
- benchmark generation
- raw saved data
- cleaning
- summary generation
- plotting

# 8. Statistics

Understand conceptually:
- AVG
- P50
- P95
- P99

P50 describes typical behavior.

P95 and P99 are important for tail latency and expose instability that the
average can hide.

The goal is to understand what each metric means, not memorize individual
benchmark values.

# 9. Data Cleaning

Study:
- `analysis/clean_pingpong.py`
- `analysis/clean_pingpong_external.py`

Understand:
- raw samples
- sorting
- percentile calculation
- trimming methodology
- summary-file generation

Distinguish RAW from CLEAN results.

# 10. Plotting

Internal:
- `analysis/plot_internal_pingpong_all_en_hq.py`

External:
- `analysis/plot_external_from_summary.py`
- `analysis/plot_external_pingpong.py`
- `analysis/plot_external_pingpong_all.py`

Understand:
- which source files are read
- whether raw CSV or summaries are used
- which methods are compared
- log-scale axes
- which plots are generated

# 11. Defense-Level Questions

For every implementation, be ready to answer:

- What problem does it solve?
- What is the exact data path?
- Where does the kernel participate?
- What synchronization is used?
- Where can context switches occur?
- What memory copies or repeated setup work exist?
- Why could this method reduce latency?
- What CPU or complexity cost does it add?
- How is RTT measured?
- Why are tail percentiles important?

For Proposed External also answer:

- Why are there multiple variants?
- Which optimization belongs to which source?
- Why can payload size affect tuning?
- What does CPU affinity change?
- What does polling change?
- What do registered buffers/files change?
- Which parts of the Linux networking path remain?
- Which files are measurement variants and which are diagnostic experiments?

# 12. The Key Mental Model

Do not memorize:

```text
Internal Proposed = shared memory
External Proposed = io_uring
```

That is too shallow.

Instead think:

```text
Internal Proposed
=
shared memory
+ ring structure
+ atomic synchronization
+ waiting strategy
+ CPU scheduling effects
```

and:

```text
External Proposed
=
I/O mechanism
+ buffer/file registration choices
+ CPU affinity
+ polling strategy
+ socket/network tuning
+ message-size-specific experimental evolution
```

That model is much closer to what the implementation actually contains.
