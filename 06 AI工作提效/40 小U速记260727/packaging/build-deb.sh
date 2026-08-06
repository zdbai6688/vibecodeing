#!/bin/bash
set -e

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build-deb"
PACKAGE_VERSION="1.1.0-1"

echo "=== 构建 UOS 速记 deb 包 ==="

# 清理
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

# 配置
/usr/bin/cmake -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTS=OFF \
    -DCMAKE_INSTALL_PREFIX=/usr

# 编译
/usr/bin/cmake --build "$BUILD_DIR" -j$(nproc)

# 修复 Whisper 共享库的 RUNPATH（嵌入式构建路径会导致加载失败）
echo "--- 修复 Whisper 共享库 RUNPATH ---"
for f in "$PROJECT_DIR"/whisper/libwhisper.so "$PROJECT_DIR"/whisper/libggml*.so*; do
    if [ -f "$f" ] && [ -x "$(command -v patchelf)" ]; then
        patchelf --remove-rpath "$f" 2>/dev/null || true
    fi
done

# 打包
cd "$BUILD_DIR"
/usr/bin/cpack -G DEB

echo "=== 构建完成 ==="
ls -lh *.deb 2>/dev/null || echo "deb 包构建完成"