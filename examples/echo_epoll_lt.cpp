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
        perror("socket"); exit(1);
    }

    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, 
        sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if(bind(listenfd, (sockaddr*)& addr, sizeof(addr)) < 0){
        perror("bind"); exit(1);
    }

    if(listen(listenfd, SOMAXCONN) < 0){
        perror("listen"); exit(1);
    }

    if(set_noblocking(listenfd) < 0){
        perror("set_noblocking"); exit(1);
    }

    return listenfd;
}

int main(){
    signal(SIGPIPE, SIG_IGN);

    int listenfd = create_listenfd(8888);
    printf("listen on 8888, listenfd=%d\n", listenfd);

    int epfd = epoll_create1(0);
    if(epfd < 0){
        perror("epoll_create1"); exit(1);
    }

    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = listenfd;
    
    if(epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev) < 0){
        perror("epoll_ctl add listenfd"); exit(1);
    }

    constexpr int MAX_EVENTS = 1024;
    epoll_event events[MAX_EVENTS];
    for(;;){
        int n = epoll_wait(epfd, events, MAX_EVENTS, -1);
        if(n < 0){
            if(errno == EINTR) continue;
            perror("epoll_wait");
            break;
        }
        for(int i = 0; i < n; i++){
            int fd = events[i].data.fd;
            if(fd == listenfd){
                
            }
            else{

            }
        }
    }

    return 0;
}