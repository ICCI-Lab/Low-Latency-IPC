# Build Instructions

## Requirements
- GCC
- GNU libc development environment
- liburing development package
- Python 3
- util-linux / taskset

## Generic Build
```bash
gcc -O3 -march=native -pipe -Wall -Wextra -o PROGRAM SOURCE.c
```

## io_uring Build
```bash
gcc -O3 -march=native -pipe -Wall -Wextra -o PROGRAM SOURCE.c -luring
```

## Examples
```bash
gcc -O3 -march=native -pipe -Wall -Wextra -o uds_pingpong uds_pingpong.c
gcc -O3 -march=native -pipe -Wall -Wextra -o pipe_pingpong pipe_pingpong.c
gcc -O3 -march=native -pipe -Wall -Wextra -o io_uring_pingpong io_uring_pingpong.c -luring
gcc -O3 -march=native -pipe -Wall -Wextra -o io_uring_tcp_client_fast64 io_uring_tcp_client_fast64.c -luring
gcc -O3 -march=native -pipe -Wall -Wextra -o ext_tcp_server_fixed ext_tcp_server_fixed.c
```

Record `gcc --version`, `uname -a`, and liburing version for reproducibility.
