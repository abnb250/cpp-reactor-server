#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

// 拼一条“cNN-#NNNN: 《payload 个 'x'>\n”， 总长=10+payload+1.
static std::string makeLine(int clientId, int seq, int payload){
    char head[32];
    std::snprintf(head, sizeof(head), "c%02d-#%04d:", clientId, seq);
    std::string line(head);
    line.append(static_cast<size_t>(payload), 'x');
    line.push_back('\n');
    return line;
}

// 循环 send 直到 n 字节全部交给内核。
static bool sendAll(int fd, const char* p, size_t n){
    while(n > 0){
        ssize_t w = ::send(fd, p, n, MSG_NOSIGNAL);
        if(w > 0){
            p += w;
            n -= static_cast<size_t>(w);
            continue;
        }
        if(w < 0 && errno == EINTR) continue;
        return false;
    }
    return true;
}

// 从 fd 读数据追加到inbuf，直到凑出一个 '\n'，取出整行并从 inbuf 删掉。
static bool readLine(int fd, std::string& inbuf, std::string& line) {
    for (;;) {
        const size_t pos = inbuf.find('\n');
        if (pos != std::string::npos) {
            line.assign(inbuf, 0, pos + 1);
            inbuf.erase(0, pos + 1);
            return true;
        }
        char tmp[8192];
        const ssize_t n = ::recv(fd, tmp, sizeof(tmp), 0);
        if (n > 0) {
            inbuf.append(tmp, static_cast<size_t>(n));
            continue;
        }
        if (n == 0) return false;
        if (errno == EINTR) continue;
        return false;
    }
}


int main(int argc, char** argv){
    const int port      = (argc > 1) ? std::atoi(argv[1]) : 8888;     
    const int clientId  = (argc > 2) ? std::atoi(argv[2]) : 0;
    const int rounds    = (argc > 3) ? std::atoi(argv[3]) : 100;
    const int payload   = (argc > 4) ? std::atoi(argv[4]) : 1400;
    const int splitMs   = (argc > 5) ? std::atoi(argv[5]) : 0;

    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0) {std::perror("socket"); return 2; }

    timeval tv{3, 0};
    ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(static_cast<uint16_t>(port));
    if(::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0){
        std::perror("connect");
        ::close(fd);
        return 2;
    }

    // ---- 阶段一：流水线发送（不等回显） ----
    // 【设计意图】真实客户端就是这样连发的；服务端一次 readv 会读回多条（粘包），
    //   这正是我们要压的形态。splitMs > 0 时把每行劈成两段，人工制造半包。
    bool sendOk = true;
    for(int seq = 1; seq <= rounds && sendOk; ++seq){
        const std::string line = makeLine(clientId, seq, payload);
        if(line.size() > 8 && splitMs > 0){
            sendOk = sendAll(fd, line.data(), 8);
            ::usleep(static_cast<useconds_t>(splitMs * 1000));
            if(sendOk) sendOk = sendAll(fd, line.data()+8, line.size() - 8);
        }
        else{
            sendOk = sendAll(fd, line.data(), line.size());
        }
    }

     // ---- 阶段二：按序号逐行校验 ----
     int ok = 0, mismatch = 0, readFailed = 0;
    std::string inbuf, line;
    for (int seq = 1; seq <= rounds && sendOk; ++seq) {
        if (!readLine(fd, inbuf, line)) {readFailed = rounds - seq + 1; break; }
        if (line == makeLine(clientId, seq, payload)) ++ok;
        else {
            if (mismatch == 0) {
                // 只打第一条，避免刷屏；编号错说明"串包"，内容短说明"半包被当成整包"
                std::printf("[client %02d] first mismatch at #%04d, got %zu bytes\n",
                            clientId, seq, line.size());
            }
            ++mismatch;
        }
    }
    

    ::close(fd);
    const int sendFailed = sendOk ? 0 : (rounds - ok - mismatch - readFailed);
    std::printf("[client %02d] ok=%d mismatch=%d readFailed=%d sendFailed=%d payload=%d splitMs=%d\n",
            clientId, ok, mismatch, readFailed, sendFailed, payload, splitMs);
    return (sendOk && mismatch == 0 && readFailed == 0) ? 0 : 1;
}