#include "net/EventLoop.h"
#include "net/Acceptor.h"
#include "net/TcpConnection.h"
#include "base/Buffer.h"

#include <netinet/in.h>
#include <signal.h>

#include <cstddef>
#include <cstdio>
#include <map>
#include <memory>
#include <string>

namespace {

using TcpConnectionPtr = reactor::TcpConnection::TcpConnectionPtr;

class EchoServer{
public:
    EchoServer(reactor::EventLoop* loop, int port) :
        loop_(loop), port_(port), acceptor_(loop, port) {

        acceptor_.setNewConnectionCallback(
            [this](int connfd, const sockaddr_in& peerAddr){
                onNewConnection(connfd, peerAddr);
            }
        );
    }

    void start(){
        std::printf("[echo] listening on 0.0.0.0:%d, listenfd=%d\n", 
                    port_, acceptor_.listenfd());
        acceptor_.listen();
    }

private:
    void onNewConnection(int connfd, const sockaddr_in& peerAddr){
        TcpConnectionPtr conn = reactor::TcpConnection::create(loop_, connfd, peerAddr);
        printf("[echo] new connection fd=%d from %s\n", connfd, 
            conn->peerAddrString().c_str());

        connections_[connfd] = conn;
        ++totalAccepted_;

        conn->setMessageCallback(
            [this](const TcpConnectionPtr& c, reactor::Buffer* buf){
                handleMessage(c, buf);
            }
        );

        conn->setCloseCallback(
            [this](const TcpConnectionPtr& c){
                handleClose(c);
            }
        );

        conn->connectEstablished();

        std::printf("[echo] +fd=%d peer=%s allive=%zu total=%zu\n", connfd,
                    conn->peerAddrString().c_str(), connections_.size(), 
                    totalAccepted_);
    }

    void handleMessage(const TcpConnectionPtr conn, reactor::Buffer* buf){
        while(conn->connected() && buf->readableBytes() > 0){
            const char* eol = buf->findEOL();
            if(eol == nullptr) break;

            const size_t len = static_cast<size_t>(eol - buf->peek()) + 1;
            const std::string line(buf->peek(), len);
            buf->retrieve(len);

            if(line == "quit\n"){
                std::printf("[echo] fd=%d asked to quit\n", conn->fd());
                conn->shutdown();
                return;
            }

            ++linesEchoed_;
            conn->send(line);
        }   
    }

    void handleClose(const TcpConnectionPtr conn){
        auto it = connections_.find(conn->fd());
        if(it != connections_.end() && it -> second == conn){
            connections_.erase(it);
        }
        std::printf("[echo] -fd=%d closed alive=%zu total=%zu lines=%zu\n", conn->fd(),
                    connections_.size(), totalAccepted_, linesEchoed_);
    }

    reactor::EventLoop* loop_;
    int port_;
    reactor::Acceptor acceptor_;
    std::map<int, TcpConnectionPtr> connections_;
    size_t totalAccepted_ = 0;
    size_t linesEchoed_ = 0;

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

