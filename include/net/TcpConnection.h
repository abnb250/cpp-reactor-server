#pragma once
#include <functional>
#include <memory>

namespace reactor{

class Channel;
class EventLoop;
class Buffer;

class TcpConnection{
public:
    using MessageCallback = std::function<void(int sockfd, Buffer*)>;
    using CloseCallback = std::function<void(int sockfd)>;

    TcpConnection(EventLoop* loop, int sockfd);
    ~TcpConnection();

    int fd() const {return fd_;}

    void setMessageCallback(MessageCallback cb){
        messageCallback_ = std::move(cb);
    }
    void setCloseCallback(CloseCallback cb){
        closeCallback_ = std::move(cb);
    }
private:
    void handleRead();
    void handleWrite();
    void handleClose();

    EventLoop* loop_;
    int fd_;
    MessageCallback messageCallback_;
    CloseCallback closeCallback_;
};

}//namespace reactor;