//构造loop， 构造channel，写入想要事件 ， 启动loop
#include "net/Channel.h"
#include "net/EventLoop.h"
#include <iostream>

int main(){
    reactor::EventLoop loop;
    reactor::Channel mock_channel(&loop, 3);

    mock_channel.enableReading();

    std::cout << "skeleton ok" << '\n';

    loop.loop();

    return 0;
}