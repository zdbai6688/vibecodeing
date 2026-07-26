#!/bin/bash
# 系统健康检查 - 支持 JSON 输出和自动修复
# Usage: bash health_check.sh [check|fix] [category]

set -euo pipefail

MODE="${1:-check}"
CATEGORY="${2:-all}"

# ============ 检查函数 ============

check_cpu() {
  local cpu_count=$(nproc 2>/dev/null || echo 1)
  local load_avg=$(uptime 2>/dev/null | awk -F'load average:' '{print $2}' | cut -d, -f1 | tr -d ' ')
  load_avg=${load_avg:-0}
  local load_num=$(echo "$load_avg" | bc 2>/dev/null || echo 0)
  local status="pass"
  local message="CPU负载正常 (${load_avg} / ${cpu_count} 核心)"
  if [ "$(echo "$load_num > $cpu_count" | bc 2>/dev/null)" = "1" ]; then
    status="warn"
    message="⚠ CPU负载过高 (${load_avg} / ${cpu_count} 核心)"
  fi
  echo "{\"name\":\"CPU负载\",\"status\":\"${status}\",\"message\":\"${message}\",\"data\":{\"load\":\"${load_avg}\",\"cores\":\"${cpu_count}\"}}"
}

check_memory() {
  local total=$(free -m 2>/dev/null | awk '/Mem:/{print $2}' || echo 0)
  local used=$(free -m 2>/dev/null | awk '/Mem:/{print $3}' || echo 0)
  local pct=0
  if [ "$total" -gt 0 ]; then
    pct=$((used * 100 / total))
  fi
  local status="pass"
  local message="内存使用正常: ${pct}% (${used}MB/${total}MB)"
  if [ "$pct" -gt 90 ]; then
    status="warn"
    message="⚠ 内存使用率过高: ${pct}%"
  elif [ "$pct" -gt 75 ]; then
    status="info"
    message="ℹ 内存使用率偏高: ${pct}%"
  fi
  echo "{\"name\":\"内存使用\",\"status\":\"${status}\",\"message\":\"${message}\",\"data\":{\"total_mb\":\"${total}\",\"used_mb\":\"${used}\",\"usage_pct\":${pct}}}"
}

check_swap() {
  local total=$(free -m 2>/dev/null | awk '/Swap:/{print $2}' || echo 0)
  local used=$(free -m 2>/dev/null | awk '/Swap:/{print $3}' || echo 0)
  local pct=0
  if [ "$total" -gt 0 ]; then
    pct=$((used * 100 / total))
  fi
  local status="pass"
  local message="Swap使用正常: ${pct}% (${used}MB/${total}MB)"
  if [ "$pct" -gt 80 ]; then
    status="warn"
    message="⚠ Swap使用率过高: ${pct}%"
  fi
  echo "{\"name\":\"Swap使用\",\"status\":\"${status}\",\"message\":\"${message}\",\"data\":{\"total_mb\":\"${total}\",\"used_mb\":\"${used}\",\"usage_pct\":${pct}}}"
}

check_disk() {
  local issues=""
  local rows=""
  while IFS= read -r line; do
    local fs=$(echo "$line" | awk '{print $1}')
    local size=$(echo "$line" | awk '{print $2}')
    local used=$(echo "$line" | awk '{print $3}')
    local avail=$(echo "$line" | awk '{print $4}')
    local usage=$(echo "$line" | awk '{print $5}' | sed 's/%//')
    local mount=$(echo "$line" | awk '{print $NF}')
    [ -z "$usage" ] && continue
    local status="pass"
    local msg="${mount}: ${usage}% (${used}/${size})"
    if [ "$usage" -gt 90 ]; then
      status="warn"
      msg="⚠ ${mount} 磁盘使用率过高: ${usage}%"
      issues="${issues}${mount}:${usage},"
    elif [ "$usage" -gt 80 ]; then
      status="info"
      msg="ℹ ${mount} 磁盘使用率偏高: ${usage}%"
    fi
    rows="${rows}{\"fs\":\"${fs}\",\"mount\":\"${mount}\",\"size\":\"${size}\",\"used\":\"${used}\",\"avail\":\"${avail}\",\"usage_pct\":${usage},\"status\":\"${status}\"}"
  done < <(df -h 2>/dev/null | grep '^/' | head -10)
  local overall="pass"
  if [ -n "$issues" ]; then overall="warn"; fi
  echo "{\"name\":\"磁盘使用\",\"status\":\"${overall}\",\"message\":\"${overall}=\"pass\"?\"✅ 磁盘使用正常\":\"⚠ 部分磁盘使用率过高\"\",\"data\":{\"partitions\":[${rows}]}}"
}

check_services() {
  local rows=""
  local issues=""
  for svc in sshd cups bluetooth avahi-daemon systemd-journald NetworkManager lightdm; do
    local active="unknown"
    local enabled="unknown"
    active=$(systemctl is-active "$svc" 2>/dev/null || echo "not_found")
    enabled=$(systemctl is-enabled "$svc" 2>/dev/null || echo "not_found")
    local status="pass"
    local msg="${svc}: ${active}"
    if [ "$active" = "inactive" ] || [ "$active" = "failed" ]; then
      if [ "$svc" = "sshd" ]; then
        status="info"
        msg="ℹ ${svc} 未运行 (可能未安装SSH服务)"
      elif [ "$svc" != "cups" ] && [ "$svc" != "bluetooth" ] && [ "$svc" != "avahi-daemon" ]; then
        status="warn"
        msg="⚠ ${svc} 未运行"
        issues="${issues}${svc},"
      fi
    fi
    rows="${rows}{\"name\":\"${svc}\",\"active\":\"${active}\",\"enabled\":\"${enabled}\",\"status\":\"${status}\",\"message\":\"${msg}\"}"
  done
  echo "{\"name\":\"关键服务\",\"status\":\"${overall:-\"pass\"}\",\"message\":\"服务检查完成\",\"data\":{\"services\":[${rows}]}}"
}

check_uptime() {
  local uptime_seconds=$(awk '{print int($1)}' /proc/uptime 2>/dev/null || echo 0)
  local days=$((uptime_seconds / 86400))
  local hours=$(( (uptime_seconds % 86400) / 3600 ))
  echo "{\"name\":\"系统运行时间\",\"status\":\"pass\",\"message\":\"系统已运行 ${days} 天 ${hours} 小时\",\"data\":{\"days\":${days},\"hours\":${hours},\"seconds\":${uptime_seconds}}}"
}

check_zombie() {
  local zombies=$(ps aux 2>/dev/null | awk '{if($8=="Z") print}' | wc -l || echo 0)
  local status="pass"
  local msg="无僵尸进程"
  if [ "$zombies" -gt 0 ]; then
    status="warn"
    msg="⚠ 发现 ${zombies} 个僵尸进程"
  fi
  echo "{\"name\":\"僵尸进程\",\"status\":\"${status}\",\"message\":\"${msg}\",\"data\":{\"count\":${zombies}}}"
}

check_temp() {
  local temp=""
  if [ -f /sys/class/thermal/thermal_zone0/temp ]; then
    temp=$(cat /sys/class/thermal/thermal_zone0/temp 2>/dev/null || echo 0)
    temp=$((temp / 1000))
  else
    temp=$(sensors 2>/dev/null | grep "Core 0" | awk '{print $3}' | tr -d '+°C' || echo "N/A")
  fi
  local status="pass"
  local msg="CPU温度: ${temp}°C"
  if [ "$temp" != "N/A" ] && [ "${temp:-0}" -gt 80 ]; then
    status="warn"
    msg="⚠ CPU温度过高: ${temp}°C"
  fi
  echo "{\"name\":\"CPU温度\",\"status\":\"${status}\",\"message\":\"${msg}\",\"data\":{\"temp\":\"${temp}\"}}"
}

# ============ 修复函数 ============

fix_disk() {
  echo "⏳ 清理磁盘空间..."
  # 清理 APT 缓存
  apt-get clean 2>/dev/null || true
  # 清理 journal 日志 (保留最近3天)
  journalctl --vacuum-time=3d 2>/dev/null || true
  # 清理临时文件
  rm -rf /tmp/* 2>/dev/null || true
  echo "✅ 磁盘清理完成 (APT缓存+旧日志+临时文件)"
}

fix_memory() {
  echo "⏳ 释放内存..."
  # 清理 PageCache
  sync && echo 1 > /proc/sys/vm/drop_caches 2>/dev/null || true
  # 调整 swappiness
  sysctl -w vm.swappiness=10 2>/dev/null || true
  echo "✅ 内存释放完成"
}

fix_services() {
  local svc="$1"
  echo "⏳ 启动服务: ${svc}..."
  systemctl start "${svc}" 2>/dev/null && echo "✅ ${svc} 已启动" || echo "❌ 启动 ${svc} 失败"
  systemctl enable "${svc}" 2>/dev/null || true
}

fix_zombie() {
  echo "⏳ 清理僵尸进程..."
  # 查找僵尸进程的父进程并发送 SIGCHLD
  ps aux | awk '{if($8=="Z") print $2}' | while read -r pid; do
    local ppid=$(ps -o ppid= -p "$pid" 2>/dev/null | tr -d ' ' || echo "")
    if [ -n "$ppid" ]; then
      kill -s SIGCHLD "$ppid" 2>/dev/null || true
    fi
  done
  echo "✅ 僵尸进程清理完成"
}

# ============ 主逻辑 ============

if [ "$MODE" = "fix" ]; then
  case "$CATEGORY" in
    disk)    fix_disk ;;
    memory)  fix_memory ;;
    sshd|cups|bluetooth|avahi-daemon|NetworkManager) fix_services "$CATEGORY" ;;
    zombie)  fix_zombie ;;
    all)
      fix_disk
      fix_memory
      fix_zombie
      echo "✅ 一键修复完成"
      ;;
    *) echo "❌ 未知修复类别: $CATEGORY"; exit 1 ;;
  esac
  exit 0
fi

# 默认模式: check - 输出 JSON
echo '{"success":true,"timestamp":"'"$(date '+%Y-%m-%d %H:%M:%S')"'","hostname":"'"$(hostname)"'","os":"'"$(uname -o) $(uname -r)"'",'
echo '"checks":['
first=true
for check in "CPU负载:check_cpu" "内存使用:check_memory" "Swap使用:check_swap" "磁盘使用:check_disk" "关键服务:check_services" "系统运行时间:check_uptime" "僵尸进程:check_zombie" "CPU温度:check_temp"; do
  name="${check%%:*}"
  func="${check##*:}"
  result=$($func)
  if [ "$first" = true ]; then first=false; else echo ","; fi
  echo -n "$result"
done
echo ']}'
