// =============================================================================
// day8_lifetime_test.cpp：验证 shared_ptr 释放 → 连接析构 → 只关一次 fd。
//
// 本文件组成（自上而下 4 块）：
//   A 头文件包含 —— Reactor 组件 + socketpair
//   B 造假连接   —— socketpair 拿一对 fd，把其中一端交给 TcpConnection
//   C 释放强引用 —— conn.reset() → ~TcpConnection → DEL → close
//   D 收尾       —— 关掉对端 fd
//
// 【设计意图】把"网络"隔离掉，只验证生命周期：use_count 归零、析构顺序、close 次数。
// =============================================================================
#include "net/EventLoop.h"
#include "net/TcpConnection.h"
#include "base/Buffer.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdio>

int main() {
    // ---- 块 B：造假连接 ----
    int fds[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) < 0) { perror("socketpair"); return 1; }
    printf("[test] socketpair: fd=%d fd=%d\n", fds[0], fds[1]);

    reactor::EventLoop loop;

    sockaddr_in peer{};
    peer.sin_family = AF_INET;
    peer.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    peer.sin_port = htons(12345);

    reactor::TcpConnection::TcpConnectionPtr conn =
        reactor::TcpConnection::create(&loop, fds[0], peer);

    conn->setMessageCallback(
        [](const reactor::TcpConnection::TcpConnectionPtr& c, reactor::Buffer* buf) {
            printf("[test] message from fd=%d: %s\n", c->fd(),
                   buf->retrieveAllAsString().c_str());
        });
    conn->setCloseCallback(
        [](const reactor::TcpConnection::TcpConnectionPtr& c) {
        printf("[test] close callback fd=%d\n", c->fd());
    });

    conn->connectEstablished();   // 注册 EPOLLIN，建立 fd→Channel→连接 的绑定
    printf("[test] use_count=%ld\n", conn.use_count());

    // ---- 块 C：释放唯一强引用 ----
    conn.reset();                 // 引用计数归零 → ~TcpConnection → DEL + close
    printf("[test] after reset: connection destroyed\n");

    // ---- 块 D：收尾 ----
    close(fds[1]);
    return 0;
}
