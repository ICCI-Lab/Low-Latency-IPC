// io_uring_tcp_server_fixed65536.c  -- io_uring echo using registered fixed buffer + fixed file
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <liburing.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static void die(const char *m){ perror(m); exit(1); }

static void pin_to_cpu(int cpu){
    if(cpu < 0) return;
    cpu_set_t s; CPU_ZERO(&s); CPU_SET(cpu,&s);
    if(sched_setaffinity(0,sizeof(s),&s) != 0) perror("sched_setaffinity");
}

static int listen_tcp(int port){
    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(fd < 0) die("socket");

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
#ifdef TCP_QUICKACK
    setsockopt(fd, IPPROTO_TCP, TCP_QUICKACK, &opt, sizeof(opt));
#endif

    struct sockaddr_in a; memset(&a,0,sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    a.sin_port = htons((uint16_t)port);

    if(bind(fd,(struct sockaddr*)&a,sizeof(a))<0) die("bind");
    if(listen(fd,1024)<0) die("listen");
    return fd;
}

int main(int argc, char **argv){
    if(argc != 4){
        fprintf(stderr,"Usage: %s <port> <msg_size> <cpu>\n", argv[0]);
        return 1;
    }
    int port = atoi(argv[1]);
    size_t msize = (size_t)strtoul(argv[2], NULL, 10);
    int cpu = atoi(argv[3]);

    signal(SIGPIPE, SIG_IGN);
    pin_to_cpu(cpu);

    int lfd = listen_tcp(port);
    int cfd = accept(lfd, NULL, NULL);
    if(cfd < 0) die("accept");

    int opt=1;
    setsockopt(cfd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
#ifdef TCP_QUICKACK
    setsockopt(cfd, IPPROTO_TCP, TCP_QUICKACK, &opt, sizeof(opt));
#endif

    struct io_uring ring;
    struct io_uring_params p; memset(&p,0,sizeof(p));
    if(io_uring_queue_init_params(256, &ring, &p) < 0) die("io_uring_queue_init_params");

    // fixed file index = 0
    int fds[1] = { cfd };
    if(io_uring_register_files(&ring, fds, 1) < 0) die("io_uring_register_files");

    void *buf = NULL;
    if(posix_memalign(&buf, 4096, msize) != 0) die("posix_memalign");
    memset(buf, 0, msize);

    struct iovec iov[1];
    iov[0].iov_base = buf;
    iov[0].iov_len  = msize;
    if(io_uring_register_buffers(&ring, iov, 1) < 0) die("io_uring_register_buffers");

    for(;;){
        struct io_uring_sqe *sqe_r = io_uring_get_sqe(&ring);
        if(!sqe_r) die("get_sqe(read_fixed)");
        io_uring_prep_read_fixed(sqe_r, 0, buf, msize, 0, 0);
        sqe_r->flags = IOSQE_FIXED_FILE;
        sqe_r->user_data = 1;

        if(io_uring_submit(&ring) < 0) die("submit(read_fixed)");

        struct io_uring_cqe *cqe = NULL;
        if(io_uring_wait_cqe(&ring, &cqe) < 0) die("wait_cqe(read_fixed)");
        int n = cqe->res;
        io_uring_cqe_seen(&ring, cqe);

        if(n <= 0) break;

        struct io_uring_sqe *sqe_w = io_uring_get_sqe(&ring);
        if(!sqe_w) die("get_sqe(write_fixed)");
        io_uring_prep_write_fixed(sqe_w, 0, buf, (unsigned)n, 0, 0);
        sqe_w->flags = IOSQE_FIXED_FILE;
        sqe_w->user_data = 2;

        if(io_uring_submit(&ring) < 0) die("submit(write_fixed)");
        if(io_uring_wait_cqe(&ring, &cqe) < 0) die("wait_cqe(write_fixed)");
        if(cqe->res < 0) { io_uring_cqe_seen(&ring,cqe); break; }
        io_uring_cqe_seen(&ring, cqe);
    }

    io_uring_unregister_buffers(&ring);
    io_uring_unregister_files(&ring);
    io_uring_queue_exit(&ring);
    close(cfd); close(lfd);
    free(buf);
    return 0;
}
