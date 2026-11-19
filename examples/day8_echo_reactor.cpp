#include "net/EventLoop.h"
#include "net/Acceptor.h"
#include "net/TcpConnection.h"
#include "base/Buffer.h"

#include <netinet/in.h>
#include <signal.h>
#include <cstdio>
#include <map>
#include <memory>
#include <string>

namespace {

using TcpConnectionPtr = reactor::TcpConnection::TcpConnectionPtr;

class EchoServer{
public:
    EchoServer(reactor::EventLoop* loop, int port) :
        loop_(loop), acceptor_(loop, port) {

        acceptor_.setNewConnectionCallback(
            [this](int connfd, const sockaddr_in& peer){
                onNewConnection(connfd, peer);
            }
        );
    }

    void start(){
        printf("[day8] echo server listening on 8888, listenfd=%d\n", acceptor_.listenfd());
        acceptor_.listen();
    }

private:
    void onNewConnection(int connfd, const sockaddr_in& peer){
        TcpConnectionPtr conn = reactor::TcpConnection::create(loop_, connfd, peer);
        printf("[day8] new connection fd=%d from %s\n", connfd, 
            conn->peerAddrString().c_str());

        connections_[connfd] = conn;

        conn->setMessageCallback(
            [](const TcpConnectionPtr& c, reactor::Buffer* buf){
                std::string msg(buf->peek(), buf->readableBytes());
                buf->retrieveAll();
                printf("[day8] conn fd = %d recv %zu bytes, echo back\n", c->fd(),
                    msg.size());
                c->send(msg);
            }
        );

        conn->setCloseCallback(
            [this](const TcpConnectionPtr& c){
                printf("[day8] connection closed fd=%d\n", c->fd());
                connections_.erase(c->fd());
            }
        );

        conn->connectEstablished();
    }

    reactor::EventLoop* loop_;
    reactor::Acceptor acceptor_;
    std::map<int, TcpConnectionPtr> connections_;

};

}// namespace

int main(){
    signal(SIGPIPE, SIG_IGN);

    reactor::EventLoop loop;
    EchoServer server(&loop, 8888);
    server.start();
    loop.loop();

    return 0;
}

