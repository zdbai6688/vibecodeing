#!/bin/bash
# 驱动管理

action="${1:-list}"

case "$action" in
  list)
    echo "=== 已安装驱动模块 ==="
    lsmod | head -40
    echo ""
    echo "=== 显卡驱动 ==="
    glxinfo -B 2>/dev/null | head -10 || echo "glxinfo 未安装"
    echo ""
    echo "=== 网络驱动 ==="
    lspci -k 2>/dev/null | grep -A 2 -i "network\|ethernet" | head -20
    echo ""
    echo "=== 硬件信息 ==="
    lspci 2>/dev/null | head -30
    ;;
  missing)
    echo "=== 检测可能缺失的驱动 ==="
    # 检测没有驱动的设备
    lspci -k 2>/dev/null | grep -B 1 "kernel driver in use" | grep -v "kernel driver" || echo "所有设备均有驱动"
    echo ""
    # 检测未使用的硬件
    lspci 2>/dev/null | grep -i "vga\|3d\|display" | head -5
    ;;
  install)
    driver="$2"
    echo "正在安装驱动: $driver"
    echo "NEED_SUDO|DEBIAN_FRONTEND=noninteractive apt-get install -y $driver"
    ;;
  remove)
    driver="$2"
    echo "正在卸载驱动: $driver"
    echo "NEED_SUDO|modprobe -r $driver"
    ;;
  *)
    echo "用法: $0 {list|missing|install|remove} [驱动名]"
    exit 1
    ;;
esac
