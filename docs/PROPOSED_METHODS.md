# Proposed Methods

This document describes the proposed implementations conceptually and records
the source variants used during development.

Numerical benchmark results are intentionally not reproduced here.

# 1. Proposed Internal Fast Path

Location:

```text
VM_1/Code/InternalFastPath/
```

Files:

```text
build.sh
consumer.c
pp_consumer.c
pp_param.c
pp_producer.c
producer.c
shm_rings.h
```

The internal proposed method explores:
- shared memory
- ring buffers
- C11 atomics
- lock-free coordination
- producer/consumer communication
- spin/yield behavior
- reduced repeated kernel-mediated communication

The important architectural distinction is that the proposed internal path
tries to exchange data through shared memory rather than repeatedly moving
each message through a conventional kernel IPC interface.

The implementation should be studied as a complete producer/consumer design:
shared data structure, synchronization, memory ordering, waiting strategy,
and benchmark harness all matter.

# 2. Proposed External Is a Separate Optimization Track

The Proposed External implementation is a major part of the project and must
not be treated as a footnote to the TCP or io_uring baselines.

It is also not represented by a single universal source file.

The directory contains several variants created for different stages of
optimization, payload behavior, scheduling experiments, IRQ-related work,
and diagnostic/KPI investigation.

## Client-side variants

```text
VM_1/Code/Proposed_External/

ext_tcp_client_fixed.c
io_uring_tcp_client_fast.c
io_uring_tcp_client_fast64.c
io_uring_tcp_client_fast512.c
io_uring_tcp_client_final.c
io_uring_tcp_client_irq.c
io_uring_tcp_client_irq_v2.c
io_uring_tcp_client_irq_final.c
io_uring_tcp_client_kpi.c
```

## Server-side variants

```text
VM_2/Code/Proposed_External/

ext_tcp_echo_server.c
ext_tcp_server_fixed.c
io_uring_tcp_server_fast.c
io_uring_tcp_server_final.c
io_uring_tcp_server_fixed65536.c
```

# 3. Why Multiple External Variants Exist

Names such as:

```text
fixed
fast
fast64
fast512
final
irq
kpi
```

have experimental meaning.

They should not be collapsed into one implementation without checking the
source.

For each variant, study four questions:

1. What problem was this variant intended to address?
2. What changed compared with the previous implementation?
3. Which Linux, io_uring, CPU, or network mechanisms does it actually use?
4. Was it used for measurement, tuning, IRQ investigation, or diagnostics?

The source file is the authority for the exact feature set.

# 4. Optimization Areas

## 4.1 io_uring

Several external variants use io_uring to control I/O submission and
completion more directly.

Important concepts:
- SQ: Submission Queue
- CQ: Completion Queue
- SQE: Submission Queue Entry
- CQE: Completion Queue Entry

The study goal is to understand where repeated userspace/kernel interaction
may be reduced and how completion handling changes compared with ordinary
blocking socket calls.

io_uring itself should not be described as automatically bypassing the whole
Linux network stack.

## 4.2 Registered / Fixed Buffers

Some variants use registered or fixed buffers.

The intention is to avoid repeating some buffer-registration or validation
work for every operation.

Do not attribute fixed-buffer behavior to every proposed file. Verify it in
the exact client/server source being discussed.

## 4.3 Registered / Fixed Files

Some variants use registered file descriptors.

This can reduce repeated descriptor-related work in io_uring submission.

Again, this is implementation-specific and should be verified from source.

## 4.4 CPU Affinity

Some benchmark runs pin execution to selected CPUs using `taskset` or
programmatic affinity.

The purpose is to reduce:
- CPU migration
- scheduler variability
- cache disruption
- run-to-run jitter

Affinity is therefore both an experimental-control technique and, in some
variants, part of the latency-tuning strategy.

## 4.5 Polling / Busy-Poll Behavior

Some variants explore polling-oriented execution.

The intention is to reduce delays related to sleeping, wakeups, interrupts,
and scheduler transitions.

The main trade-off is that lower waiting latency can require more CPU time.

## 4.6 Socket / Network Tuning

Depending on the exact variant, socket/network tuning may include mechanisms
such as:
- TCP_NODELAY
- TCP_QUICKACK
- socket buffer configuration
- SO_BUSY_POLL

These options must be explained by reading the specific source file rather
than assuming they are present everywhere.

## 4.7 IRQ-Related Variants

The `irq`, `irq_v2`, and `irq_final` client files document a separate tuning
direction related to interrupt/scheduling behavior.

They should be studied as experimental variants, not automatically treated as
the same implementation as the final measurement path.

## 4.8 KPI Variant

`io_uring_tcp_client_kpi.c` is a diagnostic/KPI-oriented variant.

Its purpose is to expose or investigate behavior useful for understanding the
system rather than merely producing another interchangeable benchmark binary.

# 5. Historical Fixed Path

One historical proposed path uses:

```bash
cd ~/thesis/Implementation/Code/Proposed_External

taskset -c 1 ./ext_tcp_server_fixed 6000 64
taskset -c 1 ./ext_tcp_server_fixed 6000 512
taskset -c 1 ./ext_tcp_server_fixed 6000 4096
taskset -c 1 ./ext_tcp_server_fixed 6000 65536
```

The matching client follows the form:

```bash
./ext_tcp_client_fixed SERVER_IP 6000 MESSAGE_SIZE 20000 2000
```

This path should be understood as one stage of the Proposed External work,
not as a label for every later optimized variant.

# 6. Message-Size-Specific Optimized Variants

The saved experiment history contains different optimized client/server
variants for different payload sizes.

This is important conceptually:

```text
Proposed External
    |
    +-- fixed path
    |
    +-- small-message optimized variants
    |
    +-- later/final variants
    |
    +-- large-message-specific tuning
    |
    +-- IRQ experiments
    |
    +-- KPI/diagnostic work
```

Therefore, when interpreting the thesis, do not memorize:

```text
Proposed External = one io_uring binary
```

Instead understand it as an optimization process in which the most suitable
implementation evolved across experimental conditions.

# 7. Data Path

Even in the proposed external method, communication is still inter-VM:

```text
VM_1 Client
    |
proposed userspace I/O implementation
    |
Linux networking components
    |
virtual NIC
    |
VMware host-only network
    |
virtual NIC
    |
Linux networking components
    |
proposed server implementation
    |
VM_2 Server
```

The proposed work optimizes parts of this path.

Do not claim complete Linux-network-stack bypass unless the exact
implementation proves it.

# 8. Result Processing

Proposed External datasets are kept separate from baseline data and are
processed by the external cleaning/summary workflow.

Conceptually:

```text
Proposed External run
        |
raw saved data
        |
clean_pingpong_external.py
        |
summary
        |
plotting / comparison
```

The purpose of this document is to explain the method and implementation
structure. Numerical benchmark results are intentionally omitted.

# 9. Study Checklist

For each Proposed External source file, be able to explain:

- client or server role
- blocking socket or io_uring path
- buffer allocation strategy
- fixed/registered buffer use, if any
- registered file use, if any
- CPU-affinity behavior
- polling/busy-poll behavior
- socket options
- completion handling
- error handling
- message-size assumptions
- experimental purpose of the variant

The most important rule is:

```text
Do not infer an optimization from the filename alone.
Verify it from the source.
```
