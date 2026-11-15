#include "net/Channel.h"
#include "net/EventLoop.h"
#include "net/EpollPoller.h"
#include <unistd.h>
#include <cstdio>

int main(){
    int fds[2];
    if(pipe(fds) < 0) {perror("pipe"); return 1;}

    reactor::EventLoop loop;
    {
        reactor::Channel ch(&loop, fds[0]);
        ch.enableReading();
        printf("fd=%d regitered\n", fds[0]);
    }
    printf("fd=%d, destoryed\n", fds[0]);
    close(fds[0]);
    close(fds[1]);
    return 0;
}