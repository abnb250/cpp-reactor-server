#include "net/Channel.h"
#include "net/EventLoop.h"

namespace reactor{

Channel::Channel(EventLoop* loop, int fd) : loop_(loop), fd_(fd) {}
Channel::~Channel() {remove(); }

void Channel::enableReading(){
    events_ |= EPOLLIN;
    update();
}

void Channel::enableWriting(){
    events_ |= EPOLLOUT;
    update();
}

void Channel::disableWriting(){
    events_ &= ~EPOLLOUT;
    update();
}

void Channel::disableAll(){
    events_ = 0;
    update();
}

void Channel::update(){
    loop_->updateChannel(this);
}

void Channel::remove(){
    loop_->removeChannel(this);
}

void Channel::handleEvent(uint32_t revents){
    revents_ = revents;

    if(revents_ & (EPOLLHUP | EPOLLERR)){
        if(closeCallback_) closeCallback_();
    }

    if(revents_ & (EPOLLIN | EPOLLPRI | EPOLLRDHUP)){
        if(readCallback_) readCallback_();
    }

    if(revents_ & EPOLLOUT){
        if(writeCallback_) writeCallback_();
    }
}

}