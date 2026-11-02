#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <iostream>

int main() {
    // 忽略 SIGPIPE，防止对端断开后 send 导致进程直接崩溃
    signal(SIGPIPE, SIG_IGN);

    // 1. 创建监听 socket (IPv4, TCP)
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        return 1;
    }

    // 2. 端口复用，防止重启服务时报 Address already in use
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 3. 绑定地址和端口
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY); // 监听所有网卡
    addr.sin_port = htons(8888);              // 端口 8888

    if (bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        perror("bind");
        close(listen_fd);
        return 1;
    }

    // 4. 开始监听
    if (listen(listen_fd, SOMAXCONN) < 0) {
        perror("listen");
        close(listen_fd);
        return 1;
    }

    std::cout << "blocking echo server listen on 0.0.0.0:8888" << std::endl;

    // 5. 循环接受客户端连接
    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        // 阻塞等待客户端连接
        int conn_fd = accept(listen_fd, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
        if (conn_fd < 0) {
            perror("accept");
            continue;
        }

        // 打印客户端 IP 和端口
        char client_ip[INET_ADDRSTRLEN] = {0};
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        std::cout << "client connected: " << client_ip << ":" << ntohs(client_addr.sin_port) << std::endl;

        // 6. 阻塞读写循环
        char buf[4096];
        while (true) {
            ssize_t n = recv(conn_fd, buf, sizeof(buf), 0);
            if (n > 0) {
                // 收到数据，原样发回 (Echo)
                ssize_t sent = 0;
                while (sent < n) {
                    ssize_t m = send(conn_fd, buf + sent, n - sent, MSG_NOSIGNAL);
                    if (m <= 0) break;
                    sent += m;
                }
            } else if (n == 0) {
                // 客户端断开
                std::cout << "client disconnected" << std::endl;
                break;
            } else {
                // 读取出错
                perror("recv");
                break;
            }
        }
        close(conn_fd);
    }

    close(listen_fd);
    return 0;
}