#include "net/Channel.h"
#include "net/EventLoop.h"
#include <iostream>

namespace reactor{

Channel::Channel(EventLoop* loop, int fd) : loop_(loop), fd_(fd) {}
Channel::~Channel() {}

void Channel::enableReading(){
    std::cout << "Channel: 准备监听 fd" << fd_ << " 的读事件" << '\n';
}

void Channel::handleEvent(uint32_t revents){
    std::cout << "Channel: fd " << fd_ <<"发生事件" << revents << '\n';
}

void Channel::update() {}

}//namespace reactor