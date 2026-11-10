//Channel: fd + 关心的事件 + 事件回调
/*
头文件保护与依赖
命名空间与前向声明
public接口
    类型别名
    构造/析构
    访问器
    回调设置
    行为方法
private区
    内部方法
    环境组
    身份组
    状态组
    回调组
命名空间收尾
*/
//我对应的fd是谁，可读时该调哪个回调，可写时该调哪个回调。

#pragma once
#include <cstdint>
#include <functional>

namespace reactor{
class EventLoop;

class Channel{
public:
    using EventCallback = std::function<void()>;

    Channel(EventLoop* loop_, int fd_);
    ~Channel();

    int fd() const {return fd_;}

    void setReadCallback(EventCallback cb){ readCallback_ = std::move(cb);}
    void setCloseCallback(EventCallback cb){ closeCallback_ = std::move(cb);}

    void enableReading();
    void handleEvent(uint32_t revents);
private:
    void update();

    EventLoop* loop_;
    int fd_;
    uint32_t events_ = 0;
    uint32_t revents_ = 0;

    EventCallback readCallback_;
    EventCallback closeCallback_;

};


}// namespace reactor