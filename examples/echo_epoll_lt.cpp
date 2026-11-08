#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <cstdlib>

static int set_noblocking(int fd){
    int flags = fcntl(fd, F_GETFL, 0);
    if(flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static int create_listenfd(int port){
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if(listenfd < 0){
        perror("listenfd");
        exit(-1);
    }

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

    if(set_noblocking(listenfd) < 0){
        perror("set_noblocking");
        exit(1);
    }

    return listenfd;
}

static void close_conn(int epfd, int fd){
    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
    close(fd);
}

static bool handle_read(int epfd, int fd){
    char buf[4096];
    for(;;){
        ssize_t n = recv(fd, buf, sizeof(buf), 0);
        if(n > 0){
            ssize_t sent = 0;
            while(sent < n){
                ssize_t w = send(fd, buf+sent, n - sent, 0);
                if(w < 0){
                    if(errno == EINTR) continue;
                    if(errno == EAGAIN || 
                        errno == EWOULDBLOCK) break;
                    return false;
                }
                sent += w;
            }
        }
        else if(n == 0){
            return false;
        }
        else if(n < 0){
            if(errno == EINTR) continue;
            if(errno == EAGAIN || errno == EWOULDBLOCK)
                return true;
            return false;
        }
    }
}

int main(){
    signal(SIGPIPE, SIG_IGN);

    int listenfd = create_listenfd(8888);
    if(listenfd < 0){
        perror("create_listenfd");
        exit(1);
    }

    int epfd = epoll_create1(0);
    if(epfd < 0){
        perror("epoll_cerate1");
        exit(1);
    }

    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = listenfd;

    if(epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev) < 0){
        perror("epoll_ctl add  listenfd");
        exit(1);
    }

    constexpr int MAXEVENTS = 1024;
    epoll_event events[MAXEVENTS];
    for(;;){
        int n = epoll_wait(epfd, events, MAXEVENTS, -1);
        if(n < 0){
            if(errno == EINTR) continue;
            perror("epoll_wait");
            break;
        }
        for(int i = 0; i < n; i++){
            int fd = events[i].data.fd;
            if(fd == listenfd){
                for(;;){
                    sockaddr_in cli{};
                    socklen_t len = sizeof(cli);
                    int connfd = accept4(listenfd, 
                        (sockaddr*)& cli, &len, SOCK_NONBLOCK);
                    if(connfd < 0){
                        if(errno == EINTR) continue;
                        if(errno == EAGAIN || 
                            errno == EWOULDBLOCK) break;
                        perror("accept4");
                        break;
                    }
                    
                    printf("new conn fd=%d from %s:%d\n", connfd,
                        inet_ntoa(cli.sin_addr), 
                        ntohs(cli.sin_port));

                    epoll_event cev{};
                    cev.events = EPOLLIN;
                    cev.data.fd = connfd;
                    if(epoll_ctl(epfd, EPOLL_CTL_ADD, connfd, &cev)
                        < 0){
                        perror("epoll_ctl add connfd");
                        close(connfd);
                    }
                }
            }
            else{
                if(!handle_read(epfd, fd)){
                    close_conn(epfd, fd);
                }
            }
        }
    }
    

    return 0;
}