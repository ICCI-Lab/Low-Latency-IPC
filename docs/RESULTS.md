# Results Organization

## Internal Matrix
Methods: UDS, Pipe, io_uring, Proposed Internal Fast Path.
Sizes: 64, 512, 4096, 65536 bytes.
Metrics: AVG, P50, P95, P99.

## External Matrix
Methods: TCP, UDP, io_uring TCP, Proposed External.
Sizes: 64, 512, 4096, 65536 bytes, with historical UDP 65000-byte data documented separately.

## Tail Latency
Average latency alone is insufficient. P95 and P99 are used to expose tail instability and latency spikes.

## Integrity Checklist
Before adding historical numeric results verify:
- source CSV
- client implementation
- server implementation
- message size
- iteration count
- warm-up parameters
- CPU affinity
- cleaning method
- VM configuration

Preserve both RAW and CLEAN statistics when available.
