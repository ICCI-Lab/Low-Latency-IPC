# Reproducibility Guide

## Environment
- VMware Workstation
- 2 Linux VMs
- 4 vCPUs per VM
- 4 GB RAM per VM
- VMnet2 host-only network
- 192.168.250.0/24
- DHCP disabled
- static IPs

## Roles
```text
VM_1 = Client + Internal IPC host
VM_2 = Server
```

## Validate
```bash
ping OTHER_VM_IP
uname -a
gcc --version
```

## Internal
```bash
cd ~/thesis/Implementation/Code/Baselines
./uds_pingpong 64
./pipe_pingpong 64
./io_uring_pingpong 64

cd ~/thesis/Implementation/Code/InternalFastPath
./PP_param 64
```
Repeat for 512, 4096, and 65536.

## External TCP
VM_2:
```bash
./tcp_pingpong_server_fixed 6000 64
```
VM_1:
```bash
./tcp_pingpong_client_fixed VM_2_SERVER_IP 6000 64 20000
```

## External UDP
VM_2:
```bash
./udp_pingpong_server 6000 64
```
VM_1:
```bash
./udp_pingpong_client VM_2_SERVER_IP 6000 64 20000
```

## External io_uring
VM_2:
```bash
./io_uring_tcp_server 6000 64
```
VM_1:
```bash
./io_uring_tcp_client VM_2_SERVER_IP 6000 64 20000
```

## Processing
```bash
cd ~/thesis/Implementation/Data
python3 clean_pingpong_external.py data_external/tcp_64B.csv
```

## Existing Thesis Results
If reproducing already validated thesis figures, do not rebuild or rerun unnecessarily. Read the existing datasets and run the existing processing/plotting scripts.

## Metadata Checklist
Record date/time, VM role, source/binary name, message size, port, iteration count, warm-up count, CPU affinity, kernel, compiler, liburing, VM resources, network configuration, raw filename, and cleaning method.
