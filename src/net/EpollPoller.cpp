#include "net/EpollPoller.h"
#include "net/Channel.h"
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>

namespace reactor{

static constexpr int kMaxEvents = 1024;

EpollPoller::EpollPoller()
    : epfd_(epoll_create1(0)), events_(kMaxEvents){
    if(epfd_ < 0){
        perror("epoll_create1");
        std::abort();
    }
}

EpollPoller::~EpollPoller(){
    if(epfd_ >= 0) close(epfd_);
}

void EpollPoller::updateChannel(Channel* channel){
    int fd = channel->fd();
    epoll_event ev{};
    ev.events = channel->events();
    ev.data.fd = fd;

    auto it = channels_.find(fd);
    if(it == channels_.end()){
        if(epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev) < 0){
            perror("epoll_ctl ADD");
            return;
        }
        channels_[fd] = channel;
    }
    else{
        if(epoll_ctl(epfd_, EPOLL_CTL_MOD, fd, &ev) < 0){
            perror("epoll_ctl mod");
        }
    }
}

void EpollPoller::removeChannel(Channel* channel){
    int fd = channel->fd();
    epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr);
    channels_.erase(fd);
}

void EpollPoller::poll(int timeoutMs, 
                        std::vector<ActiveChannel>& activeChannels){
    int n = epoll_wait(epfd_, events_.data(), 
                        static_cast<int>(events_.size()), timeoutMs);
    if(n < 0){
        if(errno == EINTR) return;
        perror("epoll_wait");
        return;
    }
    for(int i = 0; i < n; i++){
        int fd = events_[i].data.fd;
        auto it = channels_.find(fd);
        if(it != channels_.end()){
            activeChannels.push_back({channels_[fd], events_[i].events});
        }
    }
}

}//namespace reactor