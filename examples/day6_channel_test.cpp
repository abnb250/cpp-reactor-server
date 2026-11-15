#include "net/EventLoop.h"
#include "net/Channel.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>

static int set_nonblocking(int fd){
    int flags = fcntl(fd, F_GETFL, 0);
    if(flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static int create_listenfd(int port){
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if(listenfd < 0){
        perror("listenfd");
        exit(1);
    }

    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if(bind(listenfd, (sockaddr*)& addr, sizeof(addr)) < 0){
        perror("bind");
        exit(1);
    }

    if(listen(listenfd, SOMAXCONN) < 0){
        perror("listen");
        exit(1);
    }

    if(set_nonblocking(listenfd) < 0){
        perror("set_nonblocking");
        exit(1);
    }
    return listenfd;
}

static void close_connection(reactor::Channel* ch){
    int fd = ch->fd();
    ch->remove();
    close(fd);
    printf("[day6] fd=%d closed\n", fd);
}

static void make_connection(reactor::EventLoop* loop, int connfd,
                            std::vector<std::unique_ptr<reactor::Channel>>& conns){
    auto ch = std::make_unique<reactor::Channel>(loop, connfd);
    reactor::Channel* raw = ch.get();

    raw->setReadCallback([raw](){
        char buf[4096];
        ssize_t n = recv(raw->fd(), buf, sizeof(buf), 0);
        if(n > 0){
            ssize_t w = send(raw->fd(), buf, n, MSG_NOSIGNAL);
            printf("[day6] fd=%d echo %zd/%zd bytes\n", raw->fd(), w, n);
        }
        else if(n == 0){
            printf("[day6] fd=%d peer closed (recv =0)\n", raw->fd());
            close_connection(raw);
        }
        else{
            if(errno == EAGAIN || errno == EWOULDBLOCK) return;
            if(errno == EINTR) return;
            perror("recv");
            close_connection(raw);
        }
    });

    raw->setWriteCallback([raw](){
        printf("[day6] fd=%d EPOLLOUT fired, disableWriting\n", raw->fd());
        raw->disableWriting();
    });

    raw->setCloseCallback([raw](){
        printf("[day6] fd=%d EPOLLHUP/ERR\n", raw->fd());
        close_connection(raw);
    });
    
    raw->enableReading();
    raw->enableWriting();

    conns.push_back(std::move(ch));
}

int main(){
    signal(SIGPIPE, SIG_IGN);

    int listenfd = create_listenfd(8888);
    printf("[day6] listenfd=%d\n", listenfd);

    reactor::EventLoop loop;
    std::vector< std::unique_ptr<reactor::Channel> > conns;

    reactor::Channel listenChannel(&loop, listenfd);
    listenChannel.setReadCallback([&](){
        for(;;){
            sockaddr_in cli{};
            socklen_t len = sizeof(cli);
            int connfd = accept4(listenfd, (sockaddr*)& cli, &len,
                                SOCK_NONBLOCK | SOCK_CLOEXEC);
            if(connfd < 0){
                if(errno == EAGAIN || errno == EWOULDBLOCK) break;
                if(errno == EINTR) continue;
                perror("accept4");
                break;
            }
            char ipbuf[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &cli.sin_addr, ipbuf, sizeof(ipbuf));
            printf("[day6] accept connfd=%d from %s:%d\n",
                   connfd, ipbuf, ntohs(cli.sin_port));
            make_connection(&loop, connfd, conns);
        }
    });

    listenChannel.enableReading();
    printf("[day6] entering event loop...\n");
    loop.loop();
    return 0;
}

