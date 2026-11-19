#pragma once

#include "base/Buffer.h"

#include <functional>
#include <memory>
#include <netinet/in.h>
#include <string>
#include <utility>

namespace reactor{

class EventLoop;
class Channel;


class TcpConnection : public std::enable_shared_from_this<TcpConnection>{
public:
    using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
    using MessageCallback = std::function<void(const TcpConnectionPtr&, Buffer*)>;
    using CloseCallback = std::function<void(const TcpConnectionPtr&)>;

    static TcpConnectionPtr create(EventLoop* loop, int sockfd, const sockaddr_in& peerAddr);

    TcpConnection(EventLoop* loop, int sockfd, const sockaddr_in& peerAddr);
    ~TcpConnection();

    int fd() const {return fd_; };
    bool connected () const {return connected_; }
    const std::string& peerAddrString () const {return peerAddrStr_; }

    void setMessageCallback(MessageCallback cb) {messageCallback_ = std::move(cb); }
    void setCloseCallback(CloseCallback cb) {closeCallback_ = std::move(cb); }

    void connectEstablished();
    void send(const std::string& data);
    void shutdown();

private:

    void handleRead();
    void handleWrite();
    void handleError();
    void handleClose();
    void sendInLoop(const char* data, size_t len);

    EventLoop* loop_;
    int fd_;
    std::unique_ptr<Channel> channel_;

    Buffer inputBuffer_;
    Buffer outputBuffer_;

    MessageCallback messageCallback_;
    CloseCallback closeCallback_;

    bool closed_ = false;
    bool connected_ = false;
    bool fdClosed_ = false;

    std::string peerAddrStr_;
};

}//namespace reactor