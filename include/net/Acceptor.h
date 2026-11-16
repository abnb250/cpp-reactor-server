//监听fd的专属管家，只做accept这件事。
//谁要新连接，通过回调拿走fd

#pragma once
#include <functional>
#include <memory>
#include <netinet/in.h>

namespace reactor{

class EventLoop;
class Channel;
class Acceptor{
public:
    using NewConnectionCallback = std::function<void(int sockfd, sockaddr_in& peerAddr)>;

    Acceptor(EventLoop* loop, int port);
    ~Acceptor();

    int listenfd() const {return listenfd_;}
    bool listening() const {return listening_;}

    void setNewConnectionCallback(NewConnectionCallback cb);

    void listen();

private:
    void handleRead();

    static int createListenFd(int port);

    EventLoop* loop_;
    int listenfd_;

    std::unique_ptr<Channel> acceptChannel_;

    NewConnectionCallback newConnectionCallback_;

    bool listening_ = false;
};

}// namespace reactor