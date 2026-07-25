#!/bin/bash
# 系统启动项管理

case "${1:-list}" in
  list)
    echo "=== 用户自启动项 (autostart) ==="
    for f in "$HOME"/.config/autostart/*.desktop; do
      [ -f "$f" ] || continue
      name=$(grep '^Name=' "$f" 2>/dev/null | head -1 | sed 's/Name=//')
      enabled=true
      grep -qi '^Hidden=true' "$f" && enabled=false
      echo "FILE:$(basename "$f")|NAME:$name|ENABLED:$enabled"
    done
    echo "=== 系统服务启动项 ==="
    systemctl list-unit-files --type=service --no-pager 2>/dev/null | head -60 | tail -50
    ;;
  enable)
    f="$2"
    if [ -f "$f" ]; then
      sed -i 's/^Hidden=true/Hidden=false/' "$f" 2>/dev/null
      echo "已启用: $(basename "$f")"
    else
      echo "文件不存在: $f"
    fi
    ;;
  disable)
    f="$2"
    if [ -f "$f" ]; then
      if grep -qi '^Hidden=true' "$f"; then
        echo "已禁用: $(basename "$f")"
      else
        echo -e "\nHidden=true" >> "$f"
        echo "已禁用: $(basename "$f")"
      fi
    else
      echo "文件不存在: $f"
    fi
    ;;
  add)
    name="$2"
    exec="$3"
    desc="${4:-$name}"
    mkdir -p ~/.config/autostart
    cat > ~/.config/autostart/"${name// /_}.desktop" << EOF
[Desktop Entry]
Type=Application
Name=$name
Comment=$desc
Exec=$exec
X-GNOME-Autostart-enabled=true
EOF
    echo "已添加自启动项: $name"
    ;;
  remove)
    f="$2"
    rm -f "$f" 2>/dev/null
    echo "已移除: $(basename "$f")"
    ;;
  *)
    echo "用法: $0 {list|enable|disable|add|remove}"
    exit 1
    ;;
esac
