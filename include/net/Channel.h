//管理一个 fd 的关注事件（events_）和就绪事件（revents_），分发回调
//我对应的fd是谁，可读时该调哪个回调，可写时该调哪个回调。
//可读/可写/关闭或出错
#pragma once
#include <sys/epoll.h>
#include <cstdint>
#include <functional>

namespace reactor{

class EventLoop;
class Channel{
public:
    using EventCallback = std::function<void()>;

    Channel(EventLoop* loop, int fd);
    ~Channel();

    int fd() const {return fd_;}
    uint32_t events() const {return events_;}
    bool isWriting() const {return (events_ & EPOLLOUT) != 0;}

    void setReadCallback(EventCallback cb){readCallback_ = std::move(cb);}
    void setWriteCallback(EventCallback cb) {writeCallback_ = std::move(cb);}
    void setCloseCallback(EventCallback cb) {closeCallback_ = std::move(cb);}

    void enableReading();
    void enableWriting();
    void disableWriting();
    void disableAll();

    void remove();

    void handleEvent(uint32_t revents);

private:

    void update();

    EventLoop* loop_;
    int fd_;
    uint32_t events_ = 0;
    uint32_t revents_ = 0;
    EventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
};

}//namespace reactor