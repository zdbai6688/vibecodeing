#!/bin/bash
# USB启动盘制作工具 v2.0
# 功能: 列出USB设备、制作启动盘（dd写入）

action="${1:-list}"
iso_file="$2"
usb_device="$3"

list_usb_devices() {
  echo "=== 检测USB设备 ==="
  lsblk -o NAME,SIZE,TYPE,MODEL,TRAN -p -n 2>/dev/null | while read -r line; do
    name=$(echo "$line" | awk '{print $1}')
    type=$(echo "$line" | awk '{print $3}')
    tran=$(echo "$line" | awk '{print $5}')
    if [ "$type" = "disk" ] && [ "$tran" = "usb" ]; then
      size=$(echo "$line" | awk '{print $2}')
      model=$(echo "$line" | awk '{print $4}')
      echo "USB|$name|$size|${model:-USB Device}"
    fi
  done
  # Fallback: check removable flag
  for dev in /dev/sd?; do
    [ -b "$dev" ] || continue
    removable=$(cat /sys/block/$(basename "$dev")/removable 2>/dev/null)
    [ "$removable" = "1" ] || continue
    size=$(lsblk -n -o SIZE "$dev" 2>/dev/null)
    echo "USB|$dev|${size}|可移动设备"
  done 2>/dev/null | sort -u
}

case "$action" in
  list)
    list_usb_devices
    ;;
  check-iso)
    [ -f "$iso_file" ] && echo "OK|$iso_file" || echo "ERROR|ISO文件不存在: $iso_file"
    ;;
  check-device)
    [ -b "$iso_file" ] && echo "OK|$iso_file" || echo "ERROR|设备不存在: $iso_file"
    ;;
  create)
    if [ -z "$iso_file" ] || [ -z "$usb_device" ]; then
      echo "请指定ISO文件和USB设备"
      exit 1
    fi
    [ ! -f "$iso_file" ] && { echo "ISO文件不存在: $iso_file"; exit 1; }
    [ ! -b "$usb_device" ] && { echo "USB设备不存在: $usb_device"; exit 1; }
    mount | grep -q "^$usb_device" && { echo "警告: $usb_device 已挂载，请先卸载!"; exit 1; }
    echo "READY|$iso_file|$usb_device"
    ;;
  *)
    echo "用法: $0 {list|check-iso|check-device|create} [iso] [device]"
    exit 1
    ;;
esac
