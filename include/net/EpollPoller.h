//负责把 Channel 关心的事件注册/更新到 epoll，
//并在 epoll_wait 返回后把就绪 fd 反查成 Channel，
//交给 EventLoop 分发处理。
//唯一知道 epoll_ctl / epoll_wait 的地方
#pragma once
#include <cstdint>
#include <sys/epoll.h>
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

}//namespace reactor