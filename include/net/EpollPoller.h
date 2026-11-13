#pragma once
#include <sys/epoll.h>
#include <cstdint>
#include <vector>
#include <unordered_map>

namespace reactor{

class Channel;
struct ActiveChannel{
    Channel* channel;
    uint32_t revents;
};

class EpollPoller{
public:
    EpollPoller();
    ~EpollPoller();

    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);

    void poll(int timeoutMs, std::vector<ActiveChannel>& activeChannels);
private:
    int epfd_;
    std::vector<epoll_event> events_;
    std::unordered_map<int, Channel*> channels_;
};

}