#!/bin/bash
set -e

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build-linglong"

echo "=== 构建 UOS 速记 玲珑包 ==="

# 清理
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

# 使用玲珑构建
cd "$PROJECT_DIR"
ll-builder build --skip-verify-package
ll-builder export

echo "=== 玲珑包构建完成 ==="
ls -lh *.uab 2>/dev/null || ls -lh *.layer 2>/dev/null || echo "玲珑包构建完成"