//事件循环，调度 Poller 和 Channel
//事件循环，
#pragma once
#include <memory>

namespace reactor{

class Channel;
class EpollPoller;

class EventLoop{
public:
    EventLoop();
    ~EventLoop();
    void loop();
    void updateChannel(Channel* channel);
private:
    std::unique_ptr<EpollPoller> poller_;
};

}//namespace reactor