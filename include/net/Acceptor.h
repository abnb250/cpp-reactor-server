#pragma once
#include <functional>
#include <memory>

namespace reactor{

class Channel;
class EventLoop;

class Acceptor{
public:
    using NewConnectionCallback = std::function<void(int sockfd)>;

    Acceptor(EventLoop* loop, int port);
    ~Acceptor();

    void listen();
    void setNewConnectionCallback(NewConnectionCallback cb){
        newConnectionCallback_ = std::move(cb);
    }
private:
    void handleRead();
    EventLoop* loop_;
    int listenFd_ = -1;
    std::unique_ptr<Channel> acceptChannel_;
    NewConnectionCallback newConnectionCallback_;
};

}//namespace reactor;