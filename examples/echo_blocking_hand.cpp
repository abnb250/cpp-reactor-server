#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

int main(){
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if(listenfd < 0){
        perror("socket");
        return 1;
    }

    int on = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(8888);

    if(bind(listenfd, (sockaddr*)& addr, sizeof(addr)) < 0){
        perror("bind");
        return 1;
    }

    if(listen(listenfd, 128) < 0){
        perror("listen");
        return 1;
    }

    printf("listening on 8888, fd=%d\n", listenfd);

    while(true){
        sockaddr_in cli{};
        socklen_t len = sizeof(cli);
        int connfd = accept(listenfd, (sockaddr*)& cli, &len);
        if(connfd < 0){
            perror("accept");
            return 1;
        }

        char buf[4096];
        while(true){
            ssize_t r = recv(connfd, buf, sizeof(buf), 0);
            if (r < 0)
            {
                perror("recv");
                break;
            }
            else if (r == 0)
            {
                printf("connet closed, connfd=%d\n", connfd);
                break;
            }
            else
            {
                send(connfd, buf, r, 0);
            }
        }
        close(connfd);
        
    }
    close(listenfd);
    return 0;
}