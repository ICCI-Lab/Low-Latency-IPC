#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <liburing.h>

#define QUEUE_DEPTH 64

static inline long long now_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <server_ip> <port> <msg_size> <count>\n", argv[0]);
        return 1;
    }

    const char *server_ip = argv[1];
    int port = atoi(argv[2]);
    int msg_size = atoi(argv[3]);
    int count = atoi(argv[4]);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); exit(1); }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, server_ip, &addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        exit(1);
    }

    struct io_uring ring;
    io_uring_queue_init(QUEUE_DEPTH, &ring, 0);

    char *send_buf = malloc(msg_size);
    char *recv_buf = malloc(msg_size);
    memset(send_buf, 'A', msg_size);

    long long *rtts = malloc(sizeof(long long) * count);

    printf("[IOURING-CLIENT] starting... msg=%dB count=%d\n", msg_size, count);

    for (int i = 0; i < count; i++) {
        long long t0 = now_ns();

        struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
        io_uring_prep_send(sqe, sock, send_buf, msg_size, 0);
        sqe->user_data = 1;
        io_uring_submit(&ring);

        struct io_uring_cqe *cqe;
        io_uring_wait_cqe(&ring, &cqe);
        io_uring_cqe_seen(&ring, cqe);

        struct io_uring_sqe *sqe2 = io_uring_get_sqe(&ring);
        io_uring_prep_recv(sqe2, sock, recv_buf, msg_size, 0);
        sqe2->user_data = 2;
        io_uring_submit(&ring);

        io_uring_wait_cqe(&ring, &cqe);
        io_uring_cqe_seen(&ring, cqe);

        long long t1 = now_ns();
        rtts[i] = t1 - t0;
    }

    FILE *f = fopen("io_uring_tcp_pingpong.csv", "w");
    for (int i = 0; i < count; i++)
        fprintf(f, "%lld\n", rtts[i]);
    fclose(f);

    printf("[IOURING-CLIENT] CSV written: io_uring_tcp_pingpong.csv\n");

    free(send_buf);
    free(recv_buf);
    free(rtts);
    io_uring_queue_exit(&ring);
    close(sock);
    return 0;
}
