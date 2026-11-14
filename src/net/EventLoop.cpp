#include "net/EventLoop.h"
#include "net/Channel.h"
#include "net/EpollPoller.h"
#include <iostream>

namespace reactor{

EventLoop::EventLoop() : poller_(std::make_unique<EpollPoller>()) {}
EventLoop::~EventLoop() = default;

void EventLoop::loop(){
    std::vector<ActiveChannel> activeChannels;
    while(true){
        activeChannels.clear();
        poller_->poll(-1, activeChannels);
        for(auto& ac : activeChannels){
            ac.channel->handleEvent(ac.revents);
        }
    }
}

void EventLoop::updateChannel(Channel* channel){
    poller_ -> updateChannel(channel);
}

}//namespace reactor