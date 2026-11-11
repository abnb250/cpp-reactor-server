#include "net/EventLoop.h"
#include "net/Channel.h"
#include "net/EpollPoller.h"
#include <iostream>

namespace reactor{

EventLoop::EventLoop() : poller_(std::make_unique<EpollPoller>()) {}
EventLoop::~EventLoop() = default;

void EventLoop::loop(){
    std::cout<<"EventLoop: 事件循环启动（骨架版仅执行一次）" << '\n';
}

void EventLoop::updateChannel(Channel* channel){

}

}//namespace reactor