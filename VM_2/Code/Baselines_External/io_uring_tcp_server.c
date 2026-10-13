#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <liburing.h>

#define QUEUE_DEPTH  64

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <port> <msg_size>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    int msg_size = atoi(argv[2]);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); exit(1); }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind"); exit(1);
    }
    if (listen(server_fd, 1) < 0) {
        perror("listen"); exit(1);
    }

    printf("[IOURING-SERVER] listening on %d msg=%dB ...\n", port, msg_size);

    int client_fd = accept(server_fd, NULL, NULL);
    if (client_fd < 0) { perror("accept"); exit(1); }

    struct io_uring ring;
    io_uring_queue_init(QUEUE_DEPTH, &ring, 0);

    char *buf = malloc(msg_size);
    if (!buf) { perror("malloc"); exit(1); }

    while (1) {
        struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
        io_uring_prep_recv(sqe, client_fd, buf, msg_size, 0);
        sqe->user_data = 1;

        io_uring_submit(&ring);

        struct io_uring_cqe *cqe;
        io_uring_wait_cqe(&ring, &cqe);

        int ret = cqe->res;
        io_uring_cqe_seen(&ring, cqe);

        if (ret <= 0) break;

        struct io_uring_sqe *sqe2 = io_uring_get_sqe(&ring);
        io_uring_prep_send(sqe2, client_fd, buf, ret, 0);
        sqe2->user_data = 2;

        io_uring_submit(&ring);
        io_uring_wait_cqe(&ring, &cqe);
        io_uring_cqe_seen(&ring, cqe);
    }

    close(client_fd);
    close(server_fd);
    free(buf);
    io_uring_queue_exit(&ring);

    return 0;
}
