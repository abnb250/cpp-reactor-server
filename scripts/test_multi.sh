#!/bin/bash
# 多客户端并发测试：5 个客户端同时连，各自发编号
# 每个客户端发完 sleep 1 秒再断，确保服务端回显完

for i in {1..5}; do
    (echo "client-$i"; sleep 1) | nc 127.0.0.1 8888 &
done
wait
echo "=== all clients done ==="
