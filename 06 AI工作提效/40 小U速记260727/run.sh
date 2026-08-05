#!/bin/bash
# UOS速记 启动脚本
# 自动定位 whisper 共享库目录（兼容开发目录与安装目录）

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# 尝试多个候选位置
for WHISPER_DIR in \
    "$SCRIPT_DIR/whisper" \
    "$SCRIPT_DIR/../share/uos-shorthand/whisper" \
    "$SCRIPT_DIR/share/uos-shorthand/whisper"; do
    if [ -d "$WHISPER_DIR" ]; then
        export LD_LIBRARY_PATH="$WHISPER_DIR:$LD_LIBRARY_PATH"
        break
    fi
done

# 定位可执行文件
for BIN in \
    "$SCRIPT_DIR/build/uos-shorthand" \
    "$SCRIPT_DIR/uos-shorthand" \
    "$SCRIPT_DIR/../bin/uos-shorthand"; do
    if [ -x "$BIN" ]; then
        exec "$BIN" "$@"
    fi
done

echo "错误: 找不到 uos-shorthand 可执行文件"
exit 1