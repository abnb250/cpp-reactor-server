#!/bin/bash
for i in {1..5}; do
    (sleep 2) | nc 127.0.0.1 8888 &
done
wait
echo "=== all clients done ==="