#!/bin/bash
# 性能基准测试

test_type="${1:-all}"

cpu_test() {
  echo "🧪 CPU性能测试..."
  # 圆周率计算测试
  start=$(date +%s%N)
  echo "scale=5000; a(1)*4" | bc -l -q 2>/dev/null
  end=$(date +%s%N)
  duration=$(( (end - start) / 1000000 ))
  echo "RESULT:CPU:${duration}ms"
}

disk_test() {
  echo "🧪 磁盘性能测试..."
  tmpfile=$(mktemp /tmp/disk_bench.XXXXXX)
  # 顺序写入测试
  start=$(date +%s%N)
  dd if=/dev/zero of="$tmpfile" bs=1M count=512 2>&1 | grep -o '[0-9.]\+ MB/s\|[0-9.]\+ GB/s'
  end=$(date +%s%N)
  write_speed=$(( (end - start) / 1000000 ))
  # 清理
  rm -f "$tmpfile"
  echo "RESULT:DISK_WRITE:${write_speed}ms(512MB)"
}

mem_test() {
  echo "🧪 内存性能测试..."
  # 使用dd测试内存读写带宽
  start=$(date +%s%N)
  dd if=/dev/zero of=/dev/null bs=1M count=1024 2>&1 | grep -o '[0-9.]\+ MB/s\|[0-9.]\+ GB/s'
  end=$(date +%s%N)
  mem_speed=$(( (end - start) / 1000000 ))
  echo "RESULT:MEM:${mem_speed}ms(1GB)"
}

network_test() {
  echo "🧪 网络延迟测试..."
  if ping -c 3 -W 3 223.5.5.5 &>/dev/null; then
    avg=$(ping -c 3 -W 3 223.5.5.5 | tail -1 | awk -F'/' '{print $5}')
    echo "RESULT:NET:${avg}ms"
  else
    echo "RESULT:NET:超时"
  fi
}

case "$test_type" in
  cpu) cpu_test ;;
  disk) disk_test ;;
  mem) mem_test ;;
  net) network_test ;;
  all)
    cpu_test
    echo "---"
    disk_test
    echo "---"
    mem_test
    echo "---"
    network_test
    ;;
  *) echo "用法: $0 {cpu|disk|mem|net|all}" ;;
esac
