# Architecture

## Internal IPC

Internal tests execute on VM_1.

Baselines:
- Unix Domain Socket (UDS)
- Pipe
- io_uring

The proposed internal path uses shared memory, ring buffers, C11 atomics,
lock-free coordination, and spin/yield behavior.

```text
VM_1/Code/Baselines/
├── uds_pingpong.c
├── pipe_pingpong.c
└── io_uring_pingpong.c
```

```text
VM_1/Code/InternalFastPath/
├── build.sh
├── consumer.c
├── pp_consumer.c
├── pp_param.c
├── pp_producer.c
├── producer.c
└── shm_rings.h
```

Conceptually, the internal proposed method attempts to reduce repeated
kernel-mediated IPC work by moving the data exchange into shared memory and
using explicit synchronization between processes.

## External IPC

Repository convention:

```text
VM_1 = Client
VM_2 = Server
```

The external communication path remains inter-VM:

```text
VM_1 Client
    |
userspace I/O path
    |
Linux guest networking stack
    |
virtual NIC
    |
VMware host-only network
    |
virtual NIC
    |
Linux guest networking stack
    |
VM_2 Server
```

External baselines:
- TCP
- UDP
- io_uring TCP

The Proposed External work is a separate optimization track. It is not one
single program and should not be described as if every file in
`Proposed_External` uses the same combination of optimizations.

The proposed variants explore combinations of:
- io_uring
- registered/fixed buffers where implemented
- registered/fixed files where implemented
- CPU affinity
- polling-oriented behavior
- socket/network tuning
- message-size-specific tuning
- IRQ-related experiments
- KPI/diagnostic experiments

The exact feature set must be verified per source file.

## Ping-Pong Model

```text
Client -> request -> Server -> response -> Client -> record RTT
```

The benchmark compares the end-to-end behavior of baseline and proposed paths
under controlled VM and network conditions.

## Interpretation Rule

A lower-latency path should be explained in terms of architectural causes,
not merely by naming the API used.

For example, `io_uring` alone does not imply complete bypass of the Linux
network stack. The relevant explanation is which software operations,
scheduling events, memory-management steps, or repeated setup costs are
reduced by a particular implementation.
