# Architecture

## Internal IPC

Internal tests execute on VM_1. Baselines are UDS, Pipe, and io_uring. The proposed path uses shared memory, ring buffers, C11 atomics, lock-free coordination, and spin/yield behavior.

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

## External IPC

```text
VM_1 = Client
VM_2 = Server
```

Communication traverses the Linux guest networking stack, virtual NICs, and the isolated VMware host-only network. External methods are TCP, UDP, io_uring TCP, and proposed optimized variants.

## Ping-Pong Model

```text
Client -> request -> Server -> response -> Client -> record RTT
```

Common measured iteration count: 20000.
