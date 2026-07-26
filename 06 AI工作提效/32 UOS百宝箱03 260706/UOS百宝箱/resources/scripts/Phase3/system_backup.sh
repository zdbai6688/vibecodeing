#!/bin/bash
#============================================
# UOS运维工具箱 - 系统配置备份与还原脚本
#============================================

set -e

BACKUP_DIR="${HOME}/uos_backup_$(date +%Y%m%d_%H%M%S)"
BACKUP_FILE=""
ACTION="$1"

usage() {
    echo "用法: $0 {backup|restore|export|import} [选项]"
    echo ""
    echo "操作:"
    echo "  backup                  创建系统配置备份"
    echo "  restore -f <备份文件>   从备份文件还原"
    echo "  export -d <目录>        导出备份到指定目录"
    echo "  import -f <文件>        导入备份文件"
    exit 1
}

# 解析参数
parse_args() {
    while [ $# -gt 0 ]; do
        case "$1" in
            -f|--file)
                BACKUP_FILE="$2"
                shift 2
                ;;
            -d|--dir)
                BACKUP_DIR="$2"
                shift 2
                ;;
            *)
                shift
                ;;
        esac
    done
}

# 创建备份
do_backup() {
    mkdir -p "${BACKUP_DIR}"

    echo "===== 开始备份系统配置 ====="

    # 1. 备份网络配置
    echo "[1/6] 备份网络配置..."
    mkdir -p "${BACKUP_DIR}/network"
    if command -v nmcli &>/dev/null; then
        nmcli connection show > "${BACKUP_DIR}/network/connections.txt" 2>/dev/null || true
        for conn in $(nmcli -t -f NAME connection show 2>/dev/null | head -50); do
            safe_name=$(echo "$conn" | sed 's/[^a-zA-Z0-9_-]/_/g')
            nmcli connection export "$conn" > "${BACKUP_DIR}/network/${safe_name}.nmconnection" 2>/dev/null || true
        done
    fi
    # 备份 /etc/network/interfaces
    [ -f /etc/network/interfaces ] && cp /etc/network/interfaces "${BACKUP_DIR}/network/" 2>/dev/null || true
    # 备份 netplan 配置
    [ -d /etc/netplan ] && cp -r /etc/netplan "${BACKUP_DIR}/network/" 2>/dev/null || true
    # 备份 NetworkManager 配置文件
    [ -f /etc/NetworkManager/NetworkManager.conf ] && cp /etc/NetworkManager/NetworkManager.conf "${BACKUP_DIR}/network/" 2>/dev/null || true
    # 备份 hosts 文件
    [ -f /etc/hosts ] && cp /etc/hosts "${BACKUP_DIR}/network/" 2>/dev/null || true
    # 备份 DNS 配置
    [ -f /etc/resolv.conf ] && cp /etc/resolv.conf "${BACKUP_DIR}/network/" 2>/dev/null || true
    echo "网络配置备份完成"

    # 2. 备份软件源配置
    echo "[2/6] 备份软件源配置..."
    mkdir -p "${BACKUP_DIR}/sources"
    # 备份 apt 源
    cp -r /etc/apt/sources.list "${BACKUP_DIR}/sources/" 2>/dev/null || true
    [ -d /etc/apt/sources.list.d ] && cp -r /etc/apt/sources.list.d "${BACKUP_DIR}/sources/" 2>/dev/null || true
    # 备份 apt 密钥
    [ -d /etc/apt/trusted.gpg.d ] && cp -r /etc/apt/trusted.gpg.d "${BACKUP_DIR}/sources/" 2>/dev/null || true
    echo "软件源配置备份完成"

    # 3. 备份壁纸设置
    echo "[3/6] 备份壁纸设置..."
    mkdir -p "${BACKUP_DIR}/wallpaper"
    # 使用 gsettings 备份壁纸
    if command -v gsettings &>/dev/null; then
        gsettings get org.gnome.desktop.background picture-uri > "${BACKUP_DIR}/wallpaper/picture-uri.txt" 2>/dev/null || true
        gsettings get org.gnome.desktop.background picture-uri-dark > "${BACKUP_DIR}/wallpaper/picture-uri-dark.txt" 2>/dev/null || true
        gsettings get org.gnome.desktop.background picture-options > "${BACKUP_DIR}/wallpaper/picture-options.txt" 2>/dev/null || true
        gsettings get org.gnome.desktop.screensaver picture-uri > "${BACKUP_DIR}/wallpaper/screensaver-picture-uri.txt" 2>/dev/null || true
    fi
    # DConf 壁纸配置
    if command -v dconf &>/dev/null; then
        dconf dump /com/deepin/dde/appearance/ > "${BACKUP_DIR}/wallpaper/dconf-appearance.conf" 2>/dev/null || true
        dconf dump /com/deepin/dde/launcher/ > "${BACKUP_DIR}/wallpaper/dconf-launcher.conf" 2>/dev/null || true
    fi
    # 备份自定义壁纸文件
    wp_file=$(gsettings get org.gnome.desktop.background picture-uri 2>/dev/null | sed "s/^file:\/\///; s/'//g" 2>/dev/null)
    [ -n "$wp_file" ] && [ -f "$wp_file" ] && cp "$wp_file" "${BACKUP_DIR}/wallpaper/" 2>/dev/null || true
    echo "壁纸设置备份完成"

    # 4. 备份 Dock 配置
    echo "[4/6] 备份 Dock 配置..."
    mkdir -p "${BACKUP_DIR}/dock"
    if command -v gsettings &>/dev/null; then
        gsettings list-recursively com.deepin.dde.dock > "${BACKUP_DIR}/dock/dock-settings.txt" 2>/dev/null || true
        gsettings list-recursively com.deepin.dde.touchpad > "${BACKUP_DIR}/dock/touchpad-settings.txt" 2>/dev/null || true
    fi
    if command -v dconf &>/dev/null; then
        dconf dump /com/deepin/dde/dock/ > "${BACKUP_DIR}/dock/dconf-dock.conf" 2>/dev/null || true
    fi
    echo "Dock 配置备份完成"

    # 5. 备份系统主题和字体设置
    echo "[5/6] 备份系统主题和字体设置..."
    mkdir -p "${BACKUP_DIR}/theme"
    if command -v gsettings &>/dev/null; then
        gsettings get com.deepin.dde.appearance theme > "${BACKUP_DIR}/theme/theme.txt" 2>/dev/null || true
        gsettings get com.deepin.dde.appearance icon-theme > "${BACKUP_DIR}/theme/icon-theme.txt" 2>/dev/null || true
        gsettings get com.deepin.dde.appearance cursor-theme > "${BACKUP_DIR}/theme/cursor-theme.txt" 2>/dev/null || true
        gsettings get com.deepin.dde.appearance font-standard > "${BACKUP_DIR}/theme/font-standard.txt" 2>/dev/null || true
        gsettings get com.deepin.dde.appearance font-monospace > "${BACKUP_DIR}/theme/font-monospace.txt" 2>/dev/null || true
        gsettings get com.deepin.dde.appearance font-size > "${BACKUP_DIR}/theme/font-size.txt" 2>/dev/null || true
    fi
    if command -v dconf &>/dev/null; then
        dconf dump /com/deepin/dde/appearance/ > "${BACKUP_DIR}/theme/dconf-appearance.conf" 2>/dev/null || true
    fi
    echo "主题和字体设置备份完成"

    # 6. 备份系统级环境变量和别名
    echo "[6/6] 备份用户环境配置..."
    mkdir -p "${BACKUP_DIR}/env"
    [ -f ~/.bashrc ] && cp ~/.bashrc "${BACKUP_DIR}/env/" 2>/dev/null || true
    [ -f ~/.profile ] && cp ~/.profile "${BACKUP_DIR}/env/" 2>/dev/null || true
    [ -f ~/.bash_aliases ] && cp ~/.bash_aliases "${BACKUP_DIR}/env/" 2>/dev/null || true
    [ -f ~/.bash_profile ] && cp ~/.bash_profile "${BACKUP_DIR}/env/" 2>/dev/null || true
    # 备份环境变量
    env | sort > "${BACKUP_DIR}/env/environment.txt" 2>/dev/null || true
    echo "用户环境配置备份完成"

    # 打包备份
    echo ""
    echo "===== 正在打包备份文件 ====="
    local tar_file="${BACKUP_DIR}.tar.gz"
    tar -czf "$tar_file" -C "$(dirname "$BACKUP_DIR")" "$(basename "$BACKUP_DIR")" 2>/dev/null
    rm -rf "${BACKUP_DIR}"

    local size=$(du -h "$tar_file" | cut -f1)
    echo ""
    echo "✅ 备份完成: $tar_file"
    echo "   备份大小: $size"
    echo "$tar_file"
}

# 还原备份
do_restore() {
    if [ ! -f "$BACKUP_FILE" ]; then
        echo "❌ 错误: 备份文件不存在: $BACKUP_FILE"
        exit 1
    fi

    local restore_dir="${BACKUP_FILE}.extracted"
    mkdir -p "$restore_dir"

    echo "===== 开始还原系统配置 ====="
    echo "正在解压备份文件..."
    tar -xzf "$BACKUP_FILE" -C "$restore_dir"
    local actual_dir=$(ls "$restore_dir" | head -1)

    if [ -z "$actual_dir" ]; then
        echo "❌ 错误: 备份文件格式不正确"
        rm -rf "$restore_dir"
        exit 1
    fi

    local base="${restore_dir}/${actual_dir}"

    # 1. 还原网络配置
    echo "[1/6] 还原网络配置..."
    if [ -d "${base}/network" ]; then
        for conn_file in "${base}/network"/*.nmconnection; do
            if [ -f "$conn_file" ]; then
                nmcli connection import type ethernet file "$conn_file" 2>/dev/null || true
            fi
        done
        # 还原 hosts
        [ -f "${base}/network/hosts" ] && cp "${base}/network/hosts" /etc/hosts 2>/dev/null || true
        echo "网络配置还原完成"
    fi

    # 2. 还原软件源配置
    echo "[2/6] 还原软件源配置..."
    if [ -d "${base}/sources" ]; then
        [ -f "${base}/sources/sources.list" ] && cp "${base}/sources/sources.list" /etc/apt/sources.list 2>/dev/null || true
        [ -d "${base}/sources/sources.list.d" ] && cp -r "${base}/sources/sources.list.d"/* /etc/apt/sources.list.d/ 2>/dev/null || true
        echo "软件源配置还原完成"
    fi

    # 3. 还原壁纸设置
    echo "[3/6] 还原壁纸设置..."
    if [ -d "${base}/wallpaper" ]; then
        if [ -f "${base}/wallpaper/picture-uri.txt" ]; then
            local uri=$(cat "${base}/wallpaper/picture-uri.txt")
            [ -n "$uri" ] && gsettings set org.gnome.desktop.background picture-uri "$uri" 2>/dev/null || true
        fi
        if [ -f "${base}/wallpaper/picture-uri-dark.txt" ]; then
            local uri_dark=$(cat "${base}/wallpaper/picture-uri-dark.txt")
            [ -n "$uri_dark" ] && gsettings set org.gnome.desktop.background picture-uri-dark "$uri_dark" 2>/dev/null || true
        fi
        if [ -f "${base}/wallpaper/picture-options.txt" ]; then
            local opts=$(cat "${base}/wallpaper/picture-options.txt")
            [ -n "$opts" ] && gsettings set org.gnome.desktop.background picture-options "$opts" 2>/dev/null || true
        fi
        # 还原 dconf 设置
        if command -v dconf &>/dev/null; then
            [ -f "${base}/wallpaper/dconf-appearance.conf" ] && dconf load /com/deepin/dde/appearance/ < "${base}/wallpaper/dconf-appearance.conf" 2>/dev/null || true
        fi
        echo "壁纸设置还原完成"
    fi

    # 4. 还原 Dock 配置
    echo "[4/6] 还原 Dock 配置..."
    if [ -d "${base}/dock" ]; then
        if command -v dconf &>/dev/null; then
            [ -f "${base}/dock/dconf-dock.conf" ] && dconf load /com/deepin/dde/dock/ < "${base}/dock/dconf-dock.conf" 2>/dev/null || true
        fi
        echo "Dock 配置还原完成"
    fi

    # 5. 还原主题和字体设置
    echo "[5/6] 还原主题和字体设置..."
    if [ -d "${base}/theme" ]; then
        if command -v gsettings &>/dev/null && [ -f "${base}/theme/theme.txt" ]; then
            local theme_val=$(cat "${base}/theme/theme.txt")
            [ -n "$theme_val" ] && gsettings set com.deepin.dde.appearance theme "$theme_val" 2>/dev/null || true
        fi
        if [ -f "${base}/theme/icon-theme.txt" ]; then
            local icon_val=$(cat "${base}/theme/icon-theme.txt")
            [ -n "$icon_val" ] && gsettings set com.deepin.dde.appearance icon-theme "$icon_val" 2>/dev/null || true
        fi
        if [ -f "${base}/theme/font-standard.txt" ]; then
            local font_val=$(cat "${base}/theme/font-standard.txt")
            [ -n "$font_val" ] && gsettings set com.deepin.dde.appearance font-standard "$font_val" 2>/dev/null || true
        fi
        if [ -f "${base}/theme/font-monospace.txt" ]; then
            local mono_val=$(cat "${base}/theme/font-monospace.txt")
            [ -n "$mono_val" ] && gsettings set com.deepin.dde.appearance font-monospace "$mono_val" 2>/dev/null || true
        fi
        if command -v dconf &>/dev/null; then
            [ -f "${base}/theme/dconf-appearance.conf" ] && dconf load /com/deepin/dde/appearance/ < "${base}/theme/dconf-appearance.conf" 2>/dev/null || true
        fi
        echo "主题和字体设置还原完成"
    fi

    # 6. 还原环境配置
    echo "[6/6] 还原环境配置..."
    if [ -d "${base}/env" ]; then
        [ -f "${base}/env/.bashrc" ] && cp "${base}/env/.bashrc" ~/.bashrc 2>/dev/null || true
        [ -f "${base}/env/.profile" ] && cp "${base}/env/.profile" ~/.profile 2>/dev/null || true
        [ -f "${base}/env/.bash_aliases" ] && cp "${base}/env/.bash_aliases" ~/.bash_aliases 2>/dev/null || true
        echo "环境配置还原完成"
    fi

    # 清理
    rm -rf "$restore_dir"

    echo ""
    echo "✅ 系统配置还原完成!"
    echo "部分配置可能需要重新登录或重启后才能生效。"
}

# 导出备份
do_export() {
    if [ ! -f "$BACKUP_FILE" ]; then
        echo "❌ 错误: 备份文件不存在: $BACKUP_FILE"
        exit 1
    fi

    local dest_dir="${BACKUP_DIR}"
    mkdir -p "$dest_dir"

    local dest_file="${dest_dir}/uos_system_backup_$(date +%Y%m%d_%H%M%S).tar.gz"
    cp "$BACKUP_FILE" "$dest_file"

    local size=$(du -h "$dest_file" | cut -f1)
    echo "✅ 备份已导出到: $dest_file"
    echo "   文件大小: $size"
}

# 导入备份
do_import() {
    if [ ! -f "$BACKUP_FILE" ]; then
        echo "❌ 错误: 导入文件不存在: $BACKUP_FILE"
        exit 1
    fi

    local import_dir="${HOME}/uos_imported_$(date +%Y%m%d_%H%M%S)"
    mkdir -p "$import_dir"

    tar -xzf "$BACKUP_FILE" -C "$import_dir"

    local size=$(du -h "$BACKUP_FILE" | cut -f1)
    local files=$(find "$import_dir" -type f | wc -l)

    echo "✅ 导入成功!"
    echo "   文件: $BACKUP_FILE"
    echo "   大小: $size"
    echo "   配置项数: $files"
    echo "   导入路径: $import_dir"

    # 给出导入后的操作提示
    echo ""
    echo "提示: 请选择「还原」功能来应用导入的备份配置。"
}

# 主逻辑
case "$ACTION" in
    backup)
        parse_args "$@"
        do_backup
        ;;
    restore)
        parse_args "$@"
        if [ -z "$BACKUP_FILE" ]; then
            echo "❌ 错误: 请指定备份文件 (-f <文件>)"
            exit 1
        fi
        do_restore
        ;;
    export)
        parse_args "$@"
        if [ -z "$BACKUP_FILE" ]; then
            echo "❌ 错误: 请指定备份文件 (-f <文件>)"
            exit 1
        fi
        do_export
        ;;
    import)
        parse_args "$@"
        if [ -z "$BACKUP_FILE" ]; then
            echo "❌ 错误: 请指定导入文件 (-f <文件>)"
            exit 1
        fi
        do_import
        ;;
    *)
        usage
        ;;
esac
