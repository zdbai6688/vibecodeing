#!/bin/bash
# =============================================================================
# UOS百宝箱 - 自动化验证流水线
# 对 UOS百宝箱 进行自动化验证，确保代码质量和功能可用性
# =============================================================================

set -e

PROJECT_DIR="/home/ut005200@uos/06 AI工作提效/32 UOS百宝箱03 260706/UOS百宝箱"
PASS=0 FAIL=0 WARN=0 TOTAL=0
TIMESTAMP=$(date '+%Y-%m-%d %H:%M:%S')
ERROR_LOG="/tmp/uos_baibaoxiang_error.log"
: > "$ERROR_LOG"

check() {
    TOTAL=$((TOTAL+1))
    local desc="$1"
    local cmd="$2"
    echo -n "  [$TOTAL] $desc ... "
    if eval "$cmd" 2>> "$ERROR_LOG"; then
        echo "✅ 通过"
        PASS=$((PASS+1))
    else
        local exit_code=$?
        if [ "$exit_code" -eq 2 ]; then
            echo "⚠️  警告"
            WARN=$((WARN+1))
        else
            echo "❌ 失败"
            FAIL=$((FAIL+1))
        fi
    fi
}

echo "=================================================="
echo " UOS百宝箱 - 自动化验证报告"
echo " 时间: $TIMESTAMP"
echo " 项目: $PROJECT_DIR"
echo "=================================================="
echo ""

echo "--- [1/6] JS 语法检查 ---"
check "main.cjs 语法" "node --check '$PROJECT_DIR/dist-electron/main.cjs'"
check "preload.mjs 语法" "node --check '$PROJECT_DIR/dist-electron/preload.mjs'"
check "index.html 嵌入式JS语法" "node -e \"const fs=require('fs');const h=fs.readFileSync('$PROJECT_DIR/dist/index.html','utf-8');const m=h.match(/<script>([\s\S]*?)<\/script>/);if(!m) process.exit(1);fs.writeFileSync('/tmp/uos_check.js',m[1]);\" && node --check /tmp/uos_check.js"
echo ""

echo "--- [2/6] IPC 通道检查 ---"
check "ipcMain.handle 无重复注册" "grep -oP \"ipcMain\\.handle\\(\\s*'([^']+)'\" '$PROJECT_DIR/dist-electron/main.cjs' | sed \"s/ipcMain\\.handle('//\" | sort | uniq -d | head -1 | grep -q . && exit 1 || exit 0"
check "contextBridge 无重复暴露" "grep -oP \"exposeInMainWorld\\(\\s*'([^']+)'\" '$PROJECT_DIR/dist-electron/preload.mjs' | sed \"s/exposeInMainWorld('//\" | sort | uniq -d | head -1 | grep -q . && exit 1 || exit 0"
echo ""

echo "--- [3/6] 异常处理检查 ---"
check "主进程崩溃处理" "grep -qE 'uncaughtException|unhandledRejection' '$PROJECT_DIR/dist-electron/main.cjs' && exit 0; exit 2"
echo ""

echo "--- [4/6] 前端页面检查 ---"
check "核心渲染函数存在" "for fn in renderSecurity renderRepair renderSysInfo renderSysConfig renderOptimize; do grep -q \"async function \$fn\" '$PROJECT_DIR/dist/index.html' || exit 1; done; exit 0"
check "侧边栏导航项 ≥ 6" "count=\$(grep -c 'nav-item' '$PROJECT_DIR/dist/index.html' 2>/dev/null || echo 0); [ \"\$count\" -ge 6 ]"
echo ""

echo "--- [5/6] 资源完整性检查 ---"
check "resources 目录非空" "test -d '$PROJECT_DIR/resources' && ls -A '$PROJECT_DIR/resources' | grep -q ."
check "Shell 脚本语法" "find '$PROJECT_DIR/resources/scripts' -name '*.sh' 2>/dev/null | while read -r f; do bash -n \"\$f\" 2>/dev/null || exit 1; done; exit 0"
check "Python 语法" "python3 -m py_compile '$PROJECT_DIR/resources/tool_helper.py' 2>/dev/null || exit 1"
echo ""

echo "--- [6/6] 代码风格检查 ---"
check "主要使用 const/let（main.cjs）" "count=\$(grep -c '^\\s*var ' '$PROJECT_DIR/dist-electron/main.cjs' 2>/dev/null || echo 0); [ \"\$count\" -lt 5 ]"
echo ""

echo "=================================================="
echo " 验证结果: ✅ $PASS 通过, ⚠️  $WARN 警告, ❌ $FAIL 失败, 共 $TOTAL 项"
echo "=================================================="
[ -s "$ERROR_LOG" ] && { echo ""; echo "错误日志:"; cat "$ERROR_LOG"; }
[ "$FAIL" -gt 0 ] && exit 1 || exit 0
