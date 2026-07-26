#!/bin/bash
# 系统备份与还原

action="${1:-backup}"
backup_dir="${2:-$HOME/backups/$(date +%Y%m%d_%H%M%S)}"

backup() {
  mkdir -p "$backup_dir"
  echo "📁 备份目录: $backup_dir"
  
  # 备份网络配置
  echo "备份网络配置..."
  mkdir -p "$backup_dir/network"
  cp -r /etc/NetworkManager/system-connections/* "$backup_dir/network/" 2>/dev/null
  echo "OK|网络配置"
  
  # 备份APT源
  echo "备份APT源配置..."
  mkdir -p "$backup_dir/apt"
  cp -r /etc/apt/sources.list "$backup_dir/apt/" 2>/dev/null
  [ -d /etc/apt/sources.list.d ] && cp -r /etc/apt/sources.list.d/* "$backup_dir/apt/" 2>/dev/null
  echo "OK|APT源"
  
  # 备份壁纸设置
  echo "备份桌面壁纸..."
  mkdir -p "$backup_dir/desktop"
  dbus-send --session --print-reply --dest=org.freedesktop.DBus \
    /org/freedesktop/DBus org.freedesktop.DBus.ListNames 2>/dev/null | grep -q dde && {
    gsettings get com.deepin.dde.appearance background-uris 2>/dev/null > "$backup_dir/desktop/wallpaper.txt"
  } || {
    echo "壁纸文件: $(find ~/.config -name '*wall*' -o -name '*bg*' 2>/dev/null | head -5)" > "$backup_dir/desktop/wallpaper.txt"
  }
  echo "OK|壁纸设置"
  
  # 备份用户配置
  echo "备份用户配置..."
  mkdir -p "$backup_dir/user"
  cp ~/.bashrc "$backup_dir/user/" 2>/dev/null
  echo "OK|用户配置"
  
  echo "备份完成: $backup_dir"
}

restore() {
  if [ ! -d "$backup_dir" ]; then
    echo "备份目录不存在: $backup_dir"
    exit 1
  fi
  echo "📂 从 $backup_dir 恢复..."
  
  [ -d "$backup_dir/network" ] && {
    echo "恢复网络配置..."
    cp "$backup_dir/network"/* /etc/NetworkManager/system-connections/ 2>/dev/null
    echo "OK|网络配置"
  }
  
  [ -d "$backup_dir/apt" ] && {
    echo "恢复APT源配置..."
    cp "$backup_dir/apt"/sources.list /etc/apt/ 2>/dev/null
    echo "OK|APT源"
  }
  
  [ -d "$backup_dir/user" ] && {
    cp "$backup_dir/user"/.bashrc ~/ 2>/dev/null
    echo "OK|用户配置"
  }
  
  echo "✅ 恢复完成，建议重启系统以应用更改"
}

export_backup() {
  archive="/tmp/backup_$(date +%Y%m%d_%H%M%S).tar.gz"
  tar czf "$archive" -C "$(dirname "$backup_dir")" "$(basename "$backup_dir")" 2>/dev/null
  if [ -f "$archive" ]; then
    echo "EXPORT|$archive"
  fi
}

import_backup() {
  archive="$2"
  if [ ! -f "$archive" ]; then
    echo "文件不存在: $archive"
    exit 1
  fi
  import_dir="/tmp/import_backup_$$"
  mkdir -p "$import_dir"
  tar xzf "$archive" -C "$import_dir" 2>/dev/null
  echo "IMPORT|$import_dir"
}

case "$action" in
  backup) backup ;;
  restore) restore ;;
  export) export_backup ;;
  import) import_backup "$@" ;;
  *) echo "用法: $0 {backup|restore|export|import}" ;;
esac
