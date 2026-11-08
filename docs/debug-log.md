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