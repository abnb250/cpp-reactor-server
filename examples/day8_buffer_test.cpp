// =============================================================================
// day8_buffer_test.cpp：Buffer 水位与读写动作的最小验证。
//
// 本文件组成（自上而下 4 块）：
//   A 头文件包含 —— Buffer + pipe/write
//   B dump()     —— 打印三个水位和当前可读内容
//   C 内存读写   —— append / peek / retrieve
//   D 从 fd 读   —— pipe 里写 3 字节，readFd 读进 Buffer
//
// 【设计意图】先用 pipe 把"网络"这个变量隔离掉：Buffer 自己对了，
//   之后再接 TcpConnection，出问题就只可能是连接层。
// =============================================================================
#include "base/Buffer.h"
#include <unistd.h>
#include <cstdio>
#include <string>

// ---- 块 B：dump ----
static void dump(const char* tag, const reactor::Buffer& buf) {
    printf("[%s] readable=%zu writable=%zu prependable=%zu peek=\"%s\"\n",
           tag, buf.readableBytes(), buf.writableBytes(), buf.prependableBytes(),
           std::string(buf.peek(), buf.readableBytes()).c_str());
}

int main() {
    reactor::Buffer buf;
    dump("init", buf);

    // ---- 块 C：内存读写 ----
    buf.append("hello", 5);
    dump("append hello", buf);

    buf.retrieve(2);
    dump("retrieve 2", buf);

    buf.append("world", 5);
    dump("append world", buf);

    // ---- 块 D：从 fd 读 ----
    int fds[2];
    if (pipe(fds) < 0) { perror("pipe"); return 1; }
    if (write(fds[1], "abc", 3) != 3) { perror("write"); return 1; }

    int savedErrno = 0;
    ssize_t n = buf.readFd(fds[0], &savedErrno);
    printf("[readFd] n=%zd savedErrno=%d\n", n, savedErrno);
    dump("after readFd", buf);

    close(fds[0]);
    close(fds[1]);
    return 0;
}
