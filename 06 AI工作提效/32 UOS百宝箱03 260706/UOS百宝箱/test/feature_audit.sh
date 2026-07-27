#!/bin/bash
# UOS 百宝箱功能审计脚本
# 检查各功能模块的后端实现是否完整

PASS=0 FAIL=0 TOTAL=0

check() {
  TOTAL=$((TOTAL+1))
  local desc="$1"
  local cmd="$2"
  echo -n "  [$TOTAL] $desc ... "
  # 使用 bash -c 替代 eval
  if bash -c "$cmd" 2>/dev/null; then
    echo "✅"
    PASS=$((PASS+1))
  else
    echo "❌"
    FAIL=$((FAIL+1))
  fi
}

echo "=============================================="
echo " UOS 百宝箱 - 功能审计报告"
date
echo "=============================================="
echo ""

SRC="/home/ut005200@uos/06 AI工作提效/32 UOS百宝箱03 260706/UOS百宝箱"
MAIN="$SRC/dist-electron/main.cjs"
HTML="$SRC/dist/index.html"

echo "--- 1. 系统配置开关（get-sysconfig） ---"
for id in dev-mode firewall ssh bluetooth desktop-effects wifi auto-update sudo-pwfb long-filename usb-block usb-readonly cpu-mode; do
  check "$id" "grep -q \"case '$id':\" \"$MAIN\""
done

echo ""
echo "--- 2. 系统配置开关（set-sysconfig） ---"
for id in dev-mode firewall ssh bluetooth wifi sudo-pwfb cpu-mode; do
  check "$id" "grep -q \"case '$id':\" \"$MAIN\" && grep -A1 \"case '$id':\" \"$MAIN\" | grep -q 'enable\|execOut\|pkexec'"
done

echo ""
echo "--- 3. 系统优化功能 ---"
for opt in memory-tune ssd-trim avahi apt-clean disable-services fix-high-cpu clean-logs; do
  check "$opt" "grep -q \"case '$opt':\" \"$MAIN\""
done

echo ""
echo "--- 4. 高级管理（_sysmgr）---"
for action in account-policy apt-source kernel-tuning startup-mgr system-cleanup shortcut-mgr power-mgr; do
  check "$action" "grep -q \"$action\" \"$MAIN\""
done

echo ""
echo "--- 5. 故障修复（_udom）---"
for action in udom_wechat udom_wps udom_qq udom_usb udom_depends udom_input udom_store udom_print udom_launcher udom_samba_on udom_ssh_on udom_polkit udom_hostname; do
  check "$action" "grep -q \"$action\" \"$MAIN\""
done

echo ""
echo "--- 6. 安全补丁功能 ---"
check "secbase-check" "grep -q 'secbase-check' \"$MAIN\""
check "security-sync" "grep -q 'security-sync' \"$MAIN\""
check "security_sync_wrapper.py" "test -f \"$SRC/resources/security_sync/security_sync_wrapper.py\""
check "tool_helper.py" "test -f \"$SRC/resources/tool_helper.py\""

echo ""
echo "--- 7. 前端页面渲染函数 ---"
for page in renderSecApt renderSecPatch renderSecAdvisory renderSecurity renderRepair renderSysInfo renderSysConfig renderOptimize; do
  check "$page" "grep -q \"function $page\" \"$HTML\""
done

echo ""
echo "--- 8. Shell 脚本 ---"
for script in resources/scripts/SystemRepair/*.sh; do
  name=$(basename "$script")
  check "$name" "test -f \"$SRC/$script\""
done

echo ""
echo "=============================================="
echo " 审计结果: $PASS 通过, $FAIL 失败, 共 $TOTAL 项"
echo "=============================================="
