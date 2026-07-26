#!/bin/bash
# 远程桌面 - 检测VNC/RDP可用性并连接

action="${1:-check}"
vnc_host="$2"
vnc_port="${3:-5900}"

case "$action" in
  check)
    echo "=== 检查远程桌面工具 ==="
    echo "VNC客户端:"
    which vncviewer 2>/dev/null && echo "可用" || echo "未安装(tigervnc-viewer)"
    echo "RDP客户端:"
    which xfreerdp 2>/dev/null && echo "可用" || echo "未安装(freerdp2-x11)" 
    which rdesktop 2>/dev/null && echo "rdesktop可用" || true
    which krdc 2>/dev/null && echo "krdc可用" || true
    which remmina 2>/dev/null && echo "remmina可用" || true
    echo ""
    echo "VNC服务端:"
    which vncserver 2>/dev/null && echo "可用" || echo "未安装(tigervnc-standalone-server)"
    echo ""
    echo "建议安装:"
    echo "  VNC: sudo apt-get install tigervnc-viewer tigervnc-standalone-server"
    echo "  RDP: sudo apt-get install freerdp2-x11"
    ;;
  vnc_connect)
    if ! which vncviewer &>/dev/null; then
      echo "请先安装 VNC 客户端: sudo apt-get install tigervnc-viewer"
      exit 1
    fi
    if [ -z "$vnc_host" ]; then
      echo "请指定VNC服务器地址"
      exit 1
    fi
    echo "VNC_CONNECT|$vnc_host:$vnc_port"
    ;;
  rdp_connect)
    if which xfreerdp &>/dev/null; then
      echo "RDP_CONNECT|xfreerdp /v:$vnc_host:$vnc_port +clipboard"
    elif which rdesktop &>/dev/null; then
      echo "RDP_CONNECT|rdesktop $vnc_host:$vnc_port"
    else
      echo "请先安装 RDP 客户端: sudo apt-get install freerdp2-x11"
      exit 1
    fi
    ;;
  *)
    echo "用法: $0 {check|vnc_connect|rdp_connect} [host] [port]"
    exit 1
    ;;
esac
