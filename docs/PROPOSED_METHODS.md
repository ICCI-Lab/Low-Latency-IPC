# Proposed Methods

## Internal
The internal proposed method uses shared memory, ring buffers, C11 atomics, lock-free coordination, and spin/yield behavior.

Recorded benchmark:
```bash
./PP_param 64
./PP_param 512
./PP_param 4096
./PP_param 65536
```

## External Client Variants
```text
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

## External Server Variants
```text
ext_tcp_echo_server.c
ext_tcp_server_fixed.c
io_uring_tcp_server_fast.c
io_uring_tcp_server_final.c
io_uring_tcp_server_fixed65536.c
```

## Fixed Server Runs
```bash
cd ~/thesis/Implementation/Code/Proposed_External
taskset -c 1 ./ext_tcp_server_fixed 6000 64
taskset -c 1 ./ext_tcp_server_fixed 6000 512
taskset -c 1 ./ext_tcp_server_fixed 6000 4096
taskset -c 1 ./ext_tcp_server_fixed 6000 65536
```

## 64B Client
```bash
./io_uring_tcp_client_fast64 SERVER_IP 6000 64 20000 2000 2 > fast64_64B.csv
```

## 512B Client
```bash
./io_uring_tcp_client_fast512 SERVER_IP 6000 20000 2000 2 > fast512_512B.csv
```

## 4096B Final
Server:
```bash
./io_uring_tcp_server_final 6000 4096 2
```
Client:
```bash
./io_uring_tcp_client_final SERVER_IP 6000 4096 20000 2000 2 > final_4096B.csv
```

## 65536B Tuned Run
Server:
```bash
taskset -c 1 ./io_uring_tcp_server_final 6000 65536 1
```
Client:
```bash
taskset -c 2 ./io_uring_tcp_client_fast SERVER_IP 6000 65536 20000 2000 2 > tcp_pingpong_65536_fragile.csv
```

Do not treat fixed, fast, final, irq, and kpi variants as interchangeable.
