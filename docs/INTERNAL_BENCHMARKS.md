# Internal IPC Benchmarks

Primary sizes: 64, 512, 4096, 65536 bytes.

## Proposed Internal Fast Path
```bash
cd ~/thesis/Implementation/Code/InternalFastPath
./PP_param 64
./PP_param 512
./PP_param 4096
./PP_param 65536
```

## UDS
```bash
cd ~/thesis/Implementation/Code/Baselines
./uds_pingpong 64
./uds_pingpong 512
./uds_pingpong 4096
./uds_pingpong 65536
```

## Pipe
```bash
./pipe_pingpong 64
./pipe_pingpong 512
./pipe_pingpong 4096
./pipe_pingpong 65536
```

## io_uring
```bash
./io_uring_pingpong 64
./io_uring_pingpong 512
./io_uring_pingpong 4096
./io_uring_pingpong 65536
```

Metrics: AVG, P50, P95, P99. Values are normally in nanoseconds.
