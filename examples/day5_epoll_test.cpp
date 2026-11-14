//验证 EventLoop + EpollPoller + Channel 链路通了
//看看 EventLoop + EpollPoller + Channel 在监听socket中的作用。

#include "net/EventLoop.h"
#include "net/EpollPoller.h"
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

static int set_nonblocking(int fd){
    int flags = fcntl(fd, F_GETFL, 0);
    if(flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static int create_listenfd(int port){
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if(listenfd < 0){
        perror("socket");
        exit(1);
    }

    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if(bind(listenfd, (sockaddr*)& addr, sizeof(addr)) < 0 ){
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

int main(){
    signal(SIGPIPE, SIG_IGN);

    int listenfd = create_listenfd(8888);
    printf("[day5] listenfd=%d\n", listenfd);

    reactor::EventLoop loop;
    reactor::Channel listenChannel(&loop, listenfd);

    listenChannel.setReadCallback([listenfd]() {
        for(;;){
            sockaddr_in cli{};
            socklen_t len = sizeof(cli);
            int connfd = accept4(listenfd, (sockaddr*)&cli, &len, 
                                SOCK_NONBLOCK | SOCK_CLOEXEC);
            if(connfd < 0){
                if(errno == EAGAIN || errno == EWOULDBLOCK) break;
                if(errno == EINTR) continue;
                perror("accept4");
                break;
            }
            char ipbuf[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &cli.sin_addr, ipbuf, sizeof(ipbuf));
            printf("[day5] accept connfd=%d from %s::%d\n", connfd, ipbuf, ntohs(cli.sin_port));
            close(connfd);
        }
    });

    listenChannel.enableReading();
    printf("[day5] entering event loop, waitinf for connection...\n");
    loop.loop();
    return 0;
}