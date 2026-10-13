// ext_tcp_server_fixed.c  -- simple io_uring echo with registered buffer
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include <liburing.h>

static void die(const char *msg) {
    perror(msg);
    exit(1);
}

static int make_listen_socket(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) die("socket");

    int opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        die("setsockopt(SO_REUSEADDR)");

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons((uint16_t)port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        die("bind");

    if (listen(fd, 128) < 0)
        die("listen");

    return fd;
}

static void force_nodelay(int fd) {
    int flag = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <port> <msg_size>\n", argv[0]);
        return 1;
    }

    int port      = atoi(argv[1]);
    size_t msize  = strtoul(argv[2], NULL, 10);

    int lfd = make_listen_socket(port);
    printf("[server-fixed] listening on port %d, msg_size=%zu\n", port, msize);

    struct sockaddr_in cliaddr;
    socklen_t clilen = sizeof(cliaddr);
    int cfd = accept(lfd, (struct sockaddr *)&cliaddr, &clilen);
    if (cfd < 0) die("accept");

    printf("[server-fixed] client connected\n");
    force_nodelay(cfd);

    struct io_uring ring;
    struct io_uring_params params;
    memset(&params, 0, sizeof(params));

    int ret = io_uring_queue_init_params(256, &ring, &params);
    if (ret < 0) {
        fprintf(stderr, "io_uring_queue_init: %s\n", strerror(-ret));
        return 1;
    }

    // يك بافر واحد ثبت‌شده
    void *buf;
    int rc = posix_memalign(&buf, 4096, msize);
    if (rc != 0) {
        fprintf(stderr, "posix_memalign: %s\n", strerror(rc));
        return 1;
    }

    struct iovec iov[1];
    iov[0].iov_base = buf;
    iov[0].iov_len  = msize;

    ret = io_uring_register_buffers(&ring, iov, 1);
    if (ret < 0) {
        fprintf(stderr, "io_uring_register_buffers: %s\n", strerror(-ret));
        return 1;
    }

    while (1) {
        struct io_uring_sqe *sqe;
        struct io_uring_cqe *cqe;

        // RECV
        sqe = io_uring_get_sqe(&ring);
        if (!sqe) die("io_uring_get_sqe(recv)");

        io_uring_prep_read_fixed(
            sqe,
            cfd,
            buf,
            msize,
            0,
            0
        );

        ret = io_uring_submit(&ring);
        if (ret < 0) die("io_uring_submit(recv)");

        ret = io_uring_wait_cqe(&ring, &cqe);
        if (ret < 0) die("io_uring_wait_cqe(recv)");

        int n = cqe->res;
        io_uring_cqe_seen(&ring, cqe);

        if (n == 0) {
            // client closed
            break;
        }
        if (n < 0) {
            fprintf(stderr, "recv cqe->res=%d\n", n);
            break;
        }

        // SEND
        sqe = io_uring_get_sqe(&ring);
        if (!sqe) die("io_uring_get_sqe(send)");

        io_uring_prep_write_fixed(
            sqe,
            cfd,
            buf,
            n,
            0,
            0
        );

        ret = io_uring_submit(&ring);
        if (ret < 0) die("io_uring_submit(send)");

        ret = io_uring_wait_cqe(&ring, &cqe);
        if (ret < 0) die("io_uring_wait_cqe(send)");

        if (cqe->res < 0) {
            fprintf(stderr, "send cqe->res=%d\n", cqe->res);
            io_uring_cqe_seen(&ring, cqe);
            break;
        }

        io_uring_cqe_seen(&ring, cqe);
    }

    printf("[server-fixed] closing.\n");

    io_uring_unregister_buffers(&ring);
    io_uring_queue_exit(&ring);
    close(cfd);
    close(lfd);
    free(buf);

    return 0;
}

