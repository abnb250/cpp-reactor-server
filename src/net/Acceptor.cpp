#include "net/Acceptor.h"
#include "net/EventLoop.h"
#include "net/Channel.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>

namespace reactor{

static int setNonBlocking(int fd){
    int flags = fcntl(fd, F_GETFL, 0);
    if(flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int Acceptor::createListenFd(int port){
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

    if(bind(listenfd, (sockaddr*)& addr, sizeof(addr)) < 0){
        perror("bind");
        exit(1);
    }

    if(::listen(listenfd, SOMAXCONN) < 0){
        perror("listen");
        exit(1);
    }

    if(setNonBlocking(listenfd) < 0){
        perror("setNonBlocking");
        exit(1);
    }

    return listenfd;
}

Acceptor::Acceptor(EventLoop* loop, int port) : loop_(loop), 
    listenfd_(createListenFd(port)), 
    acceptChannel_(std::make_unique<Channel>(loop, listenfd_)) {

    acceptChannel_->setReadCallback( [this](){ handleRead(); } );
}

Acceptor::~Acceptor(){
    acceptChannel_.reset();
    close(listenfd_);
}

void Acceptor::setNewConnectionCallback(NewConnectionCallback cb){
    newConnectionCallback_ = std:: move(cb);
}

void Acceptor::listen(){
    listening_ = true;
    acceptChannel_->enableReading();
}

void Acceptor::handleRead(){
    for(;;){
        sockaddr_in peer{};
        socklen_t len = sizeof(peer);

        int connfd = accept4(listenfd_, (sockaddr*)&peer, &len,
                            SOCK_NONBLOCK | SOCK_CLOEXEC);
        
        if(connfd < 0){
            if(errno == EAGAIN || errno == EWOULDBLOCK) break;
            if(errno == EINTR) continue;
            perror("accept4");
            break;
        }

        if(newConnectionCallback_){
            newConnectionCallback_(connfd, peer);
        }
        else{
            close(connfd);
        }
    }
}

}// namespace reactor