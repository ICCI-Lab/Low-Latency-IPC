# External IPC Benchmarks

```text
VM_1 = Client
VM_2 = Server
```

## General Order
1. Start server on VM_2.
2. Confirm listening.
3. Run matching client on VM_1.
4. Wait for completion.
5. Preserve/rename generated CSV.
6. Repeat for next size.

## TCP Baseline

VM_2:
```bash
cd ~/thesis/Implementation/Code/Baselines_External
./tcp_pingpong_server_fixed 6000 64
./tcp_pingpong_server_fixed 6000 512
./tcp_pingpong_server_fixed 6000 4096
./tcp_pingpong_server_fixed 6000 65536
```

VM_1:
```bash
./tcp_pingpong_client_fixed SERVER_IP 6000 64 20000
./tcp_pingpong_client_fixed SERVER_IP 6000 512 20000
./tcp_pingpong_client_fixed SERVER_IP 6000 4096 20000
./tcp_pingpong_client_fixed SERVER_IP 6000 65536 20000
```

## UDP Baseline
VM_2:
```bash
./udp_pingpong_server 6000 64
./udp_pingpong_server 6000 512
./udp_pingpong_server 6000 4096
./udp_pingpong_server 6000 65536
```
VM_1:
```bash
./udp_pingpong_client SERVER_IP 6000 64 20000
./udp_pingpong_client SERVER_IP 6000 512 20000
./udp_pingpong_client SERVER_IP 6000 4096 20000
./udp_pingpong_client SERVER_IP 6000 65536 20000
```
Historical large UDP run:
```bash
./udp_pingpong_server 6000 65000
./udp_pingpong_client SERVER_IP 6000 65000 20000
```

## io_uring TCP Baseline
VM_2:
```bash
./io_uring_tcp_server 6000 64
./io_uring_tcp_server 6000 512
./io_uring_tcp_server 6000 4096
./io_uring_tcp_server 6000 65536
```
VM_1:
```bash
./io_uring_tcp_client SERVER_IP 6000 64 20000
./io_uring_tcp_client SERVER_IP 6000 512 20000
./io_uring_tcp_client SERVER_IP 6000 4096 20000
./io_uring_tcp_client SERVER_IP 6000 65536 20000
```

## Cleaning
```bash
cd ~/thesis/Implementation/Data
python3 clean_pingpong_external.py data_external/tcp_64B.csv
```
Use the equivalent command for other datasets.
