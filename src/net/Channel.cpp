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

    EventCallback readCb = readCallback_;
    EventCallback writeCb = writeCallback_;
    EventCallback closeCb = closeCallback_;

    if(revents & (EPOLLHUP | EPOLLERR)){
        if(closeCb) closeCb();
        return;
    }

    if(revents & (EPOLLIN | EPOLLPRI | EPOLLRDHUP)){
        if(readCb) readCb();
    }

    if(revents & EPOLLOUT){
        if(writeCb) writeCb();
    }
}

}