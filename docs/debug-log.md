# Debug Log

## Day 1：阻塞 echo server 第二个客户端无回显

- 现象：第一个 nc 连接后收发正常；第二个 nc 能连上，但发数据没有回显。
- 原因：服务端 accept 后进入内层 recv 循环，阻塞处理第一个连接，没有回到 accept。
- 结论：阻塞 IO + 单线程只能串行服务一个连接。
- 下一步：改成非阻塞 + epoll LT，单线程同时处理多个客户端。

## Day1 nc 命令把 IP 和端口写成点号导致“连不上”
- 现象：`nc 127.0.0.1.8888` 报 `missing port number`，误以为服务端 accept 有问题。
- 原因：IP 和端口之间必须用空格或冒号，点号会被当成主机名。
- 修复：`nc 127.0.0.1 8888`，accept/recv/send 全部正常。
- 教训：客户端连不上，先看客户端命令，再看服务端。

---

## Day2 epoll_create1(0) 写成 epoll_create(0)
- 现象：程序启动即打印 `epoll_create: Invalid argument` 并退出
- 定位：strace 确认调的是 epoll_create(0)，返回 -1 EINVAL
- 原因：epoll_create 的 size 参数是历史遗留，但内核要求必须 > 0；
        传 0 非法。epoll_create1 的参数是 flags，才可以传 0
- 修复：改用 `epoll_create1(0)`
- 教训：epoll_create 和 epoll_create1 名字像、参数语义完全不同；
        perror 前缀要和实际调用一致，否则日志会误导排查方向

---

## day3 重启服务器报 Address already in use
### 现象
第一次 ./build/echo_epoll_lt 正常；Ctrl+C 后立刻再跑，
bind 失败：`bind: Address already in use`，进程退出。
等约 60 秒后再跑又正常。

### 排查过程
1. 查端口占用：`ss -tanp | grep 8888`  
   输出显示一条 TIME_WAIT 状态的连接，本地端口 8888  
   TIME-WAIT 0      0           127.0.0.1:8888     127.0.0.1:51928   
2. 查 TIME_WAIT 来源：上一个服务器进程主动 close 连接后，
   内核保留该四元组 2×MSL（Linux 约 60 秒），防止旧包干扰新连接
3. 对照 man 7 socket：SO_REUSEADDR 允许 bind 忽略
   TIME_WAIT 占用，但必须在 bind 之前设置

### 根因
监听 socket 未设 SO_REUSEADDR。TIME_WAIT 期间
(ip, port) 仍被内核占用，bind 返回 EADDRINUSE。

### 修复
在 bind 之前：
    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

### 验证
恢复 setsockopt 后，连续「启动→Ctrl+C→启动」10 次，
均正常 bind，无 EADDRINUSE。ss 仍能看到 TIME_WAIT，
但 bind 不再受阻。

---

cat >> docs/debug-log.md <<'EOF'
## Day 6: Channel 析构未 remove 导致 epoll 残留

### 现象
连接关闭后 close(fd)，但没从 epoll 摘除。客户端反复连接/断开 100 次后，
偶发段错误，GDB backtrace 指向 Channel::handleEvent，但该 Channel 已析构。

### 定位
strace -e epoll_ctl 观察，close(fd) 之后没有 EPOLL_CTL_DEL。
fd 被内核复用给新连接后，epoll_wait 把事件投给旧 Channel 的 data.fd，
EpollPoller 查 channels_ 拿到悬空指针 → UAF。

### 修复
~Channel() 调用 remove()；EpollPoller::removeChannel 加“未注册直接返回”判断。

### 验证
strace 确认 EPOLL_CTL_DEL 出现在 close 之前；反复连接 100 次 ASan 无报错。
EOF

---

## Day 8：客户端断开时服务端 heap-use-after-free（Channel 已被回调析构）

### 现象

单客户端 echo 正常。客户端 Ctrl+C 断开后，服务端必现崩溃，ASan 报：

```text
ERROR: AddressSanitizer: heap-use-after-free
READ of size 4 at 0x... thread T0
    #0 reactor::Channel::handleEvent(unsigned int)  Channel.cpp:52
    #1 reactor::EventLoop::loop()                  EventLoop.cpp:17
freed by thread T0 here:
    #4 reactor::TcpConnection::~TcpConnection()    TcpConnection.cpp:33
    #13 operator()                                 TcpConnection.cpp:46
```

`0x...` 位于一个 120 字节的 Channel 对象内部偏移 16 处。

### 复现

```bash
./build/bin/day8_echo_reactor
# 另开终端
nc 127.0.0.1 8888
hello        # 有回显
# Ctrl+C 断开客户端 → 服务端 ASan 报错并 abort
```

### 定位过程

1. `strace` 抓关闭路径，顺序正常：

   ```text
   readv(5, [...], 2) = 0          ← 对端 FIN
   epoll_ctl(3, EPOLL_CTL_DEL, 5, NULL) = 0
   close(5) = 0
   ```

   说明 `handleClose` 的“DEL → close → 通知上层”没写反。

2. 看 ASan 的两段栈：

   - **释放点**：`TcpConnection.cpp:46` 的读回调 lambda 结束 → 局部 `conn`（shared_ptr）析构 → 引用计数归零 → `~TcpConnection` → `channel_.reset()` → `~Channel`；
   - **访问点**：`Channel::handleEvent` 第 52 行，在 `readCb()` 返回之后。

   即：**Channel 在它自己的 handleEvent 还没返回时就被析构了。**

3. 对照 `Channel.cpp`：

   ```cpp
   void Channel::handleEvent(uint32_t revents) {
       revents_ = revents;
       EventCallback readCb = readCallback_;
       // ...
       if (revents_ & (EPOLLHUP | EPOLLERR)) { if (closeCb) closeCb(); }
       if (revents_ & (EPOLLIN | ...)) { if (readCb) readCb(); }
       if (revents_ & EPOLLOUT) { if (writeCb) writeCb(); }   // ← 第 52 行
   }
   ```

   回调虽然已经拷贝到局部，但**条件判断用的还是成员 `revents_`**。读 `revents_` 就等于访问 `this`，而 `this` 已经没了。

4. 另一个问题：`EPOLLHUP | EPOLLERR` 分支处理完没有 `return`，同轮如果还带 `EPOLLIN`，会在已关闭的连接上继续派发。

### 根因

完整调用链：

```text
Channel::handleEvent
  └─ readCallback_()
       └─ lambda: auto conn = weak.lock(); conn->handleRead();
            recv 返回 0
            → handleClose()
                 → channel_->remove()          // EPOLL_CTL_DEL
                 → close(fd)
                 → closeCallback_(shared_from_this())
                      → EchoServer: connections_.erase(fd)   // 丢掉 map 里的强引用
            ← handleRead 返回
       ← lambda 返回，局部 conn 析构，引用计数归零
            → ~TcpConnection → channel_.reset() → ~Channel
  ← 回到 handleEvent，继续读 revents_ → heap-use-after-free
```

`weak_ptr` 只保证“**调用前**连接对象还活着”，它管不了“**回调返回后** Channel 自己还活着”。拷贝回调解决了“回调对象还在不在”，但没解决“`this` 还在不在”。

### 修复

`Channel::handleEvent` 分发期间不碰任何成员：

```cpp
void Channel::handleEvent(uint32_t revents) {
    revents_ = revents;              // 只在最前面写一次

    EventCallback readCb  = readCallback_;
    EventCallback writeCb = writeCallback_;
    EventCallback closeCb = closeCallback_;

    if (revents & (EPOLLHUP | EPOLLERR)) {
        if (closeCb) closeCb();
        return;                      // 错误/挂断处理完立即返回
    }
    if (revents & (EPOLLIN | EPOLLPRI | EPOLLRDHUP)) {
        if (readCb) readCb();
    }
    if (revents & EPOLLOUT) {
        if (writeCb) writeCb();
    }
}
```

两个要点：

1. **条件一律用参数 `revents`**，它在线程栈上，对象析构也不受影响；
2. `closeCb` 之后 `return`，避免在已关闭连接上继续派发。

`TcpConnection` 侧保持原设计：

- 三个回调捕获 `weak_ptr`，`lock()` 判活；
- 关闭顺序 `remove() → close(fd) → closeCallback_(erase)`；
- `closed_` 防重复关闭，`fdClosed_` 防重复 close。

### 验证

```bash
# 1. 单连接 100 次连断，ASan 无报错
for i in {1..100}; do (sleep 0.1) | timeout 1 nc 127.0.0.1 8888 & done
wait

# 2. DEL 在 close 之前，且单次连接只 close 一次
strace -f -e trace=epoll_ctl,close -o /tmp/day8.strace ./build/bin/day8_echo_reactor
grep -c 'close(5)' /tmp/day8.strace      # 期望 1

# 3. fd 不泄漏
PID=$(pgrep -f 'build/bin/day8_echo_reactor' | head -n1)
ls /proc/$PID/fd | wc -l                 # 压测前后一致
```

### 复盘（面试可以这么讲）

“我在 Day 8 遇到过一个 UAF：客户端断开时，读回调里 `handleClose` 通知上层 `erase`，连接引用计数归零，`~TcpConnection` 把成员 `Channel` 也析构了；但此刻 `Channel::handleEvent` 还在栈上，回调返回后它继续读成员 `revents_`，访问的就是已释放内存。修复是让 `handleEvent` 先把三个回调拷到局部、条件只用参数 `revents`、错误分支直接 `return`，保证分发期间不碰 `this`。这件事让我分清了两层生命周期：`weak_ptr` 保证的是连接对象在回调执行期间活着，而分发函数自己的 `this` 是否有效，必须由分发逻辑自己保证。”

### 排查中踩的两个坑

1. **`printf` 不带 `\n`**：stdout 行缓冲，日志一直留在缓冲区里，看起来像“服务端没收到数据”，其实是没刷新。定位阶段一度误判成读事件没触发。
2. **改完源码没重编**：ASan 报告里的 BuildId 和上一次完全一样，说明跑的仍是旧二进制。以后看 BuildId 或 `ls -l build/bin/...` 的时间戳，确认新程序真的生效。

---

## day9: sizeof 用在指针或容器上永远是“类型大小”，不是“数据长度”
现象：客户端只发出每条消息的前 32 字节，服务端回显无 '\n'，readLine 永远等不到整行。
定位：sendAll(fd, line.data(), sizeof(line)) —— sizeof(std::string) == 32。
修复：改为 line.size()。
教训：sizeof 只对裸数组/内置类型等于长度；对指针是 8，对容器是对象大小。

---

## Day 9: 一次可读被当成一条消息，一条命令被处理两遍

### 现象
压测客户端 ok/mismatch 全绿，看不出问题。但在 MessageCallback 里加一行探针
打印 readableBytes() 后发现：一条 1411 字节的消息被内核拆成 8 + 1403 两次可读事件，
MessageCallback 被调用两次，服务端也回发了两次。
把业务从"整块回显"改成"按 '\n' 切行"之后，第一次那 8 字节里没有 '\n'，
这条消息直接卡住不回声，客户端超时，压测才暴露出 failed。

### 定位
`MessageCallback` 里直接 `peek()` + `retrieveAll()` 并回发，隐含假设"一次可读 == 一条消息"。
Day 8 的 echo 在字节层面是透明的（8 + 1403 拼起来仍然正确），所以这个假设一直被掩盖；
一旦业务需要"按消息处理"（心跳、长度协议、登录请求），半包就会被当成一条独立命令。

### 修复
1. Buffer 增加 findEOL()，业务循环"切出一整行才处理"；
2. 切不出 '\n' 就 break，剩余字节留在 inputBuffer_ 等下一次可读事件；
3. 连接表 erase 前用 it->second == conn 做身份校验，防止 fd 复用删错连接；
4. handleRead/handleWrite 加 shared_from_this() guard，防止回调中途析构后继续访问成员。

### 验证
splitMs=20 的半包压测 20 条全 ok；服务端 lines 计数 = 消息条数（不是事件次数）；
strace 确认 EPOLL_CTL_DEL 在 close 之前且 close 只出现一次；40 个短连接后 fd 数回落。