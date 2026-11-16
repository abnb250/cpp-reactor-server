#include "net/EventLoop.h"
#include "net/Acceptor.h"

#include <arpa/inet.h>
#include <cstdio>
#include <unistd.h>

// printf("acceptor listening on 8888, listenfd=%d\n", ;
// printf("new conn fd=%d from %s:%d\n", );

int main(){
    reactor::EventLoop loop;
    reactor::Acceptor acceptor(&loop, 8888);

    printf("acceptor listening on 8888, listenfd=%d\n", acceptor.listenfd());

    acceptor.setNewConnectionCallback([](int connfd, sockaddr_in& peer){
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &peer.sin_addr, ip, sizeof(ip));
        printf("new conn fd=%d from %s:%d\n",connfd, ip, ntohs(peer.sin_port) );

        close(connfd);
    });

    acceptor.listen();
    loop.loop();

    return 0;
}