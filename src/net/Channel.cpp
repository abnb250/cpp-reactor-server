#include "net/Channel.h"
#include "net/EventLoop.h"
#include <iostream>

namespace reactor{

Channel::Channel(EventLoop* loop, int fd) : loop_(loop), fd_(fd) {}
Channel::~Channel() {}

void Channel::enableReading(){
    events_ |= EPOLLIN;
    update();
}

void Channel::handleEvent(uint32_t revents){
    revents_ = revents;
    if(revents_ & EPOLLIN){
        if(readCallback_) readCallback_();
    }
}

void Channel::update() {
    loop_->updateChannel(this);
}

}//namespace reactor