# Low-Latency IPC

Source code, experimental setup, benchmark procedures, and reproducibility documentation for a master's thesis investigating low-latency inter-process communication on Linux.

## Repository VM Convention

```text
VM_1 = Client + Internal IPC benchmark machine
VM_2 = Server
```

## Evaluated Methods

### Internal IPC
- Unix Domain Socket (UDS)
- Pipe
- io_uring
- Proposed shared-memory fast path

### External IPC
- TCP
- UDP
- io_uring-based TCP
- Proposed optimized external implementations

## Message Sizes
- 64 B
- 512 B
- 4096 B
- 65536 B

A historical UDP run used 65000 bytes for the largest datagram.

## Metrics
- AVG
- P50
- P95
- P99

## Documentation
- [Architecture](docs/ARCHITECTURE.md)
- [Experimental Environment](docs/ENVIRONMENT.md)
- [Build Instructions](docs/BUILD.md)
- [Internal Benchmarks](docs/INTERNAL_BENCHMARKS.md)
- [External Benchmarks](docs/EXTERNAL_BENCHMARKS.md)
- [Proposed Methods](docs/PROPOSED_METHODS.md)
- [Data Processing](docs/DATA_PROCESSING.md)
- [Results](docs/RESULTS.md)
- [Reproducibility](docs/REPRODUCIBILITY.md)

Generated binaries, raw outputs, temporary files, archives, and generated plots are excluded from version control.
