#include "net/TcpConnection.h"
#include "net/EventLoop.h"
#include "net/Channel.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <cstring>

namespace reactor{

using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
using MessageCallback = std::function<void(const TcpConnection&, Buffer*)>;
using CloseCallback = std::function<void(const TcpConnection&)>;

TcpConnectionPtr TcpConnection::create(EventLoop* loop, int sockfd, const sockaddr_in& peerAddr){
    return std::make_shared<TcpConnection>(loop, sockfd, peerAddr);
}

TcpConnection::TcpConnection(EventLoop* loop, int sockfd, const sockaddr_in& peerAddr) : 
    loop_(loop), 
    fd_(sockfd),
    channel_(std::make_unique<Channel>(loop, sockfd)) {

    char ip[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &peerAddr.sin_addr, ip, sizeof(ip));
    peerAddrStr_ = std::string(ip) + ":" + std::to_string(ntohs(peerAddr.sin_port));
}

TcpConnection::~TcpConnection(){
    channel_.reset();
    if(!fdClosed_){
        ::close(fd_);
        fdClosed_ = true;
    }
    fprintf(stderr, "[day8] ~TcpConnection fd=%d destoryed \n", fd_);
}

void TcpConnection::connectEstablished(){
    connected_ = true;
    std::weak_ptr<TcpConnection> weak = weak_from_this();

    channel_->setReadCallback([weak](){
        if(auto conn = weak.lock()) conn->handleRead();
    });
    channel_->setWriteCallback([weak](){
        if(auto conn = weak.lock()) conn->handleWrite();
    });
    channel_->setCloseCallback([weak](){
        if(auto conn = weak.lock()) conn->handleError();
    });

    channel_->enableReading();
}

void TcpConnection::shutdown(){
    TcpConnectionPtr guard = shared_from_this();
    if(connected_) handleClose();
}

void TcpConnection::handleRead(){
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(fd_, &savedErrno);
    if(n > 0){
        if(messageCallback_) messageCallback_(shared_from_this(), &inputBuffer_);
    }
    else if(n == 0){
        handleClose();
    }
    else{
        if(savedErrno == EAGAIN || savedErrno == EWOULDBLOCK || savedErrno == EINTR) return;
        handleError();
    }
}

void TcpConnection::send(const std::string& data){
    if(!connected_) return;
    sendInLoop(data.data(), data.size());
}

void TcpConnection::sendInLoop(const char* data, size_t len){
    ssize_t nwrote = 0;
    if(outputBuffer_.readableBytes() == 0){
        nwrote = ::send(fd_, data, len, MSG_NOSIGNAL);
        if(nwrote < 0){
            nwrote = 0;
            if(errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR){
                handleError();
                return;
            }
        }
    }

    if(static_cast<size_t>(nwrote) < len){
        outputBuffer_.append(data + nwrote, len - static_cast<size_t>(nwrote));
        if(!channel_->isWriting()) channel_->enableWriting();
    }

}

void TcpConnection::handleWrite(){
    if(outputBuffer_.readableBytes() == 0){
        channel_->disableWriting();
        return;
    }

    ssize_t n = ::send(fd_, outputBuffer_.peek(), outputBuffer_.readableBytes(),
                        MSG_NOSIGNAL);
    if(n > 0){
        outputBuffer_.retrieve(static_cast<size_t>(n));
        if(outputBuffer_.readableBytes() == 0){
            channel_->disableWriting();
        }
    }
    else if(n < 0){
        if(errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
            handleError();
    }
}

void TcpConnection::handleError() {
    int err = 0;
    socklen_t len = sizeof(err);
    ::getsockopt(fd_, SOL_SOCKET, SO_ERROR, &err, &len);
    if(err != 0){
        fprintf(stderr, "fd = %d socket error %s\n", fd_, std::strerror(err));
    }
    handleClose();
}

void TcpConnection::handleClose(){
    if(closed_) return;
    closed_ = true;
    connected_ = false;

    channel_->remove();

    if(!fdClosed_){
        ::close(fd_);
        fdClosed_ = true;
    }

    if(closeCallback_) closeCallback_(shared_from_this());
}

}//namespace reactor;