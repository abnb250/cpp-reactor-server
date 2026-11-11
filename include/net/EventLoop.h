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