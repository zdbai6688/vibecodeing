#!/bin/bash
# 系统性能分析工具
# Usage: perf_analyze.sh cpu|disk|memory|cpuhotspot|memoryleak|strace|report|perf [args]

ACTION="$1"
shift

case "$ACTION" in
  cpu)
    echo "{"
    echo '"success":true,'
    echo '"type":"cpu",'
    echo '"timestamp":"'$(date '+%Y%m%d_%H%M%S')'",'
    # CPU 核心数
    cores=$(nproc 2>/dev/null || echo 0)
    echo '"cores":'$cores','
    # CPU 型号
    model=$(grep -m1 'model name' /proc/cpuinfo 2>/dev/null | sed 's/.*: //' | sed 's/"/\\\\"/g' || echo "unknown")
    echo '"model":"'$model'",'
    # CPU 使用率（1秒平均）
    cpu_idle=$(top -bn1 2>/dev/null | grep '%Cpu' | awk '{print $8}' | head -1 || echo 0)
    cpu_usage=$(echo "scale=1; 100 - $cpu_idle" | bc 2>/dev/null || echo 0)
    echo '"usage_percent":'$cpu_usage','
    # CPU 负载
    load=$(cat /proc/loadavg 2>/dev/null | awk '{print "\"1min\":"$1",\"5min\":"$2",\"15min\":"$3}')
    echo '"load":{'"$load"'}'
    echo "}"
    ;;

  disk)
    echo "{"
    echo '"success":true,'
    echo '"type":"disk",'
    echo '"timestamp":"'$(date '+%Y%m%d_%H%M%S')'",'
    # 磁盘信息
    echo -n '"disks":['
    first=1
    df -h 2>/dev/null | grep '^/' | while read line; do
      dev=$(echo "$line" | awk '{print $1}')
      size=$(echo "$line" | awk '{print $2}')
      used=$(echo "$line" | awk '{print $3}')
      avail=$(echo "$line" | awk '{print $4}')
      pct=$(echo "$line" | awk '{print $5}')
      mount=$(echo "$line" | awk '{print $6}')
      if [ $first -eq 1 ]; then first=0; else echo -n ','; fi
      echo -n '{"device":"'"$dev"'","size":"'"$size"'","used":"'"$used"'","avail":"'"$avail"'","use_percent":"'"$pct"'","mount":"'"$mount"'"}'
    done
    echo '],'
    # I/O 统计
    echo -n '"io_stats":['
    first=1
    iostat -x 1 2 2>/dev/null | tail -20 | while read line; do
      d=$(echo "$line" | awk '{print $1}')
      r=$(echo "$line" | awk '{print $6}')
      w=$(echo "$line" | awk '{print $7}')
      [ -z "$d" ] && continue
      [ "$d" = "Device" ] && continue
      [ "$d" = "Linux" ] && continue
      if [ $first -eq 1 ]; then first=0; else echo -n ','; fi
      echo -n '{"device":"'"$d"'","r_await":"'"$r"'","w_await":"'"$w"'"}'
    done 2>/dev/null
    echo ']'
    echo "}"
    ;;

  memory)
    echo "{"
    echo '"success":true,'
    echo '"type":"memory",'
    echo '"timestamp":"'$(date '+%Y%m%d_%H%M%S')'",'
    # 内存信息
    total=$(free -m 2>/dev/null | grep 'Mem:' | awk '{print $2}' || echo 0)
    used=$(free -m 2>/dev/null | grep 'Mem:' | awk '{print $3}' || echo 0)
    avail=$(free -m 2>/dev/null | grep 'Mem:' | awk '{print $7}' || echo 0)
    swap_total=$(free -m 2>/dev/null | grep 'Swap:' | awk '{print $2}' || echo 0)
    swap_used=$(free -m 2>/dev/null | grep 'Swap:' | awk '{print $3}' || echo 0)
    pct=$(echo "scale=1; $used * 100 / $total" | bc 2>/dev/null || echo 0)
    echo '"total_mb":'$total','
    echo '"used_mb":'$used','
    echo '"available_mb":'$avail','
    echo '"swap_total_mb":'$swap_total','
    echo '"swap_used_mb":'$swap_used','
    echo '"usage_percent":'$pct','
    # Top 内存进程
    echo -n '"top_processes":['
    first=1
    ps aux --sort=-%mem 2>/dev/null | head -11 | tail -10 | while read line; do
      pid=$(echo "$line" | awk '{print $2}')
      user=$(echo "$line" | awk '{print $1}')
      mem_pct=$(echo "$line" | awk '{print $4}')
      cmd=$(echo "$line" | awk '{$1=$2=$3=$4=""; print substr($0,5)}' | cut -c1-50 | sed 's/"/\\\\"/g')
      if [ $first -eq 1 ]; then first=0; else echo -n ','; fi
      echo -n '{"pid":'$pid',"user":"'$user'","mem_pct":"'$mem_pct'","cmd":"'$cmd'"}'
    done
    echo ']'
    echo "}"
    ;;

  cpuhotspot)
    # CPU 热点分析 - 使用 perf
    echo "{"
    echo '"success":true,'
    echo '"type":"cpuhotspot",'
    echo '"timestamp":"'$(date '+%Y%m%d_%H%M%S')'",'
    # 检查 perf 是否可用
    perf_available=$(which perf 2>/dev/null && echo "yes" || echo "no")
    echo '"perf_available":"'$perf_available'",'
    if [ "$perf_available" = "yes" ]; then
      # 使用 perf top 快速采样（非交互模式，5秒采样）
      PERF_OUTPUT=$(perf top -b -n 20 -d 1 -e cycles -E 10 2>&1 | head -30)
      echo '"perf_top":"'$(echo "$PERF_OUTPUT" | sed 's/"/\\\\"/g' | sed ':a;N;$!ba;s/\n/\\n/g')'",'
      # 获取当前 CPU 使用率最高的进程
      echo -n '"top_cpu_procs":['
      first=1
      ps aux --sort=-%cpu 2>/dev/null | head -11 | tail -10 | while read line; do
        pid=$(echo "$line" | awk '{print $2}')
        user=$(echo "$line" | awk '{print $1}')
        cpu_pct=$(echo "$line" | awk '{print $3}')
        cmd=$(echo "$line" | awk '{$1=$2=$3=$4=$5=$6=$7=$8=$9=$10=""; print substr($0,11)}' | cut -c1-60 | sed 's/"/\\\\"/g')
        if [ $first -eq 1 ]; then first=0; else echo -n ','; fi
        echo -n '{"pid":'$pid',"user":"'$user'","cpu_pct":'$cpu_pct',"cmd":"'$cmd'"}'
      done
      echo ']'
    else
      # perf 不可用，使用 ps 分析
      echo -n '"top_cpu_procs":['
      first=1
      ps aux --sort=-%cpu 2>/dev/null | head -11 | tail -10 | while read line; do
        pid=$(echo "$line" | awk '{print $2}')
        user=$(echo "$line" | awk '{print $1}')
        cpu_pct=$(echo "$line" | awk '{print $3}')
        cmd=$(echo "$line" | awk '{$1=$2=$3=$4=$5=$6=$7=$8=$9=$10=""; print substr($0,11)}' | cut -c1-60 | sed 's/"/\\\\"/g')
        if [ $first -eq 1 ]; then first=0; else echo -n ','; fi
        echo -n '{"pid":'$pid',"user":"'$user'","cpu_pct":'$cpu_pct',"cmd":"'$cmd'"}'
      done
      echo ']'
    fi
    echo "}"
    ;;

  memoryleak)
    # 内存泄漏检测 - 分析系统内存增长趋势和进程内存映射
    echo "{"
    echo '"success":true,'
    echo '"type":"memoryleak",'
    echo '"timestamp":"'$(date '+%Y%m%d_%H%M%S')'",'
    # 内核内存统计
    slab_total=$(cat /proc/meminfo 2>/dev/null | grep 'Slab:' | awk '{print $2}' || echo 0)
    s_unreclaim=$(cat /proc/meminfo 2>/dev/null | grep 'SUnreclaim:' | awk '{print $2}' || echo 0)
    echo '"slab_total_kb":'$slab_total','
    echo '"slab_unreclaim_kb":'$s_unreclaim','
    # 缓存统计
    cached=$(cat /proc/meminfo 2>/dev/null | grep '^Cached:' | awk '{print $2}' || echo 0)
    buffers=$(cat /proc/meminfo 2>/dev/null | grep '^Buffers:' | awk '{print $2}' || echo 0)
    echo '"cached_kb":'$cached','
    echo '"buffers_kb":'$buffers','
    # 匿名页统计 (可能泄漏)
    anon_pages=$(cat /proc/meminfo 2>/dev/null | grep 'AnonPages:' | awk '{print $2}' || echo 0)
    echo '"anon_pages_kb":'$anon_pages','
    # Top RSS 进程 (可能为内存泄漏嫌疑进程)
    echo -n '"suspect_processes":['
    first=1
    ps aux --sort=-%rss 2>/dev/null | head -11 | tail -10 | while read line; do
      pid=$(echo "$line" | awk '{print $2}')
      user=$(echo "$line" | awk '{print $1}')
      rss=$(echo "$line" | awk '{print $6}')
      mem_pct=$(echo "$line" | awk '{print $4}')
      cmd=$(echo "$line" | awk '{$1=$2=$3=$4=$5=$6=$7=$8=$9=$10=""; print substr($0,11)}' | cut -c1-60 | sed 's/"/\\\\"/g')
      # 将 RSS 转换为 MB
      rss_mb=$(echo "scale=1; $rss / 1024" | bc 2>/dev/null || echo 0)
      if [ $first -eq 1 ]; then first=0; else echo -n ','; fi
      echo -n '{"pid":'$pid',"user":"'$user'","rss_mb":'$rss_mb',"mem_pct":"'$mem_pct'","cmd":"'$cmd'"}'
    done
    echo ']'
    echo "}"
    ;;

  strace)
    # strace 系统调用跟踪
    echo "{"
    echo '"success":true,'
    echo '"type":"strace",'
    echo '"timestamp":"'$(date '+%Y%m%d_%H%M%S')'",'
    strace_available=$(which strace 2>/dev/null && echo "yes" || echo "no")
    echo '"strace_available":"'$strace_available'",'
    if [ "$strace_available" = "yes" ]; then
      PID="${1:-}"
      if [ -z "$PID" ]; then
        # 如果没有指定 PID，获取 CPU 使用率最高的进程 PID
        PID=$(ps aux --sort=-%cpu 2>/dev/null | head -2 | tail -1 | awk '{print $2}')
        echo '"auto_pid":'$PID','
      fi
      echo '"target_pid":'$PID','
      # 快速跟踪：统计系统调用频次（2秒采样）
      STRACE_OUTPUT=$(timeout 3 strace -c -p $PID 2>&1 | head -30)
      echo '"strace_summary":"'$(echo "$STRACE_OUTPUT" | sed 's/"/\\\\"/g' | sed ':a;N;$!ba;s/\n/\\n/g')'"'
    else
      echo '"strace_summary":"strace 工具未安装"'
    fi
    echo "}"
    ;;

  report)
    # 生成综合性能分析报告
    REPORT_TIME=$(date '+%Y%m%d_%H%M%S')
    HOSTNAME=$(hostname 2>/dev/null || echo "unknown")
    REPORT_FILE="/tmp/perf_report_${HOSTNAME}_${REPORT_TIME}.txt"

    {
      echo "============================================"
      echo "  UOS 系统性能分析报告"
      echo "  生成时间: $(date '+%Y-%m-%d %H:%M:%S')"
      echo "  主机名: $HOSTNAME"
      echo "============================================"
      echo ""
      echo "【系统基本信息】"
      echo "  OS: $(grep PRETTY_NAME /etc/os-release 2>/dev/null | cut -d= -f2 | tr -d '\"' || echo 'UOS')"
      echo "  内核: $(uname -r 2>/dev/null || echo 'unknown')"
      echo "  CPU: $(nproc) 核心"
      grep -m1 'model name' /proc/cpuinfo 2>/dev/null | sed 's/model name/  型号/'
      echo "  内存: $(free -h 2>/dev/null | grep 'Mem:' | awk '{print $2}') 总量 / $(free -h | grep 'Mem:' | awk '{print $3}') 已用"
      echo ""
      echo "【CPU 分析】"
      echo "  使用率: $(top -bn1 2>/dev/null | grep '%Cpu' | awk '{print 100-$8"%"}')"
      echo "  负载: $(cat /proc/loadavg 2>/dev/null | awk '{print $1", "$2", "$3}')"
      echo ""
      echo "【CPU 热点进程 Top 10】"
      ps aux --sort=-%cpu 2>/dev/null | head -11 | awk '{printf "  %-8s %-6s %-6s %s\n", $2, $1, $3"%", $11}'
      echo ""
      echo "【内存分析】"
      free -h 2>/dev/null
      echo ""
      echo "【内存消耗 Top 10 进程】"
      ps aux --sort=-%rss 2>/dev/null | head -11 | awk '{printf "  %-8s %-6s %-8s %-5s %s\n", $2, $1, $6"KB", $4"%", $11}'
      echo ""
      echo "【磁盘使用率】"
      df -h 2>/dev/null | grep '^/' | awk '{printf "  %-20s %-8s %-8s %-5s %s\n", $1, $2, $3, $5, $6}'
      echo ""
      echo "【磁盘 I/O 统计】"
      iostat -x 1 2 2>/dev/null | tail -10
      echo ""
      echo "【系统内存泄漏风险评估】"
      slab=$(cat /proc/meminfo 2>/dev/null | grep 'Slab:' | awk '{print $2" "$3}' || echo 'N/A')
      echo "  Slab 内存: $slab"
      anon=$(cat /proc/meminfo 2>/dev/null | grep 'AnonPages:' | awk '{print $2" "$3}' || echo 'N/A')
      echo "  匿名页: $anon"
      if [ -f /proc/meminfo ]; then
        mem_available=$(grep 'MemAvailable:' /proc/meminfo 2>/dev/null | awk '{print $2}')
        mem_total=$(grep 'MemTotal:' /proc/meminfo 2>/dev/null | awk '{print $2}')
        if [ -n "$mem_available" ] && [ -n "$mem_total" ] && [ "$mem_total" -gt 0 ]; then
          avail_pct=$(echo "scale=1; $mem_available * 100 / $mem_total" | bc)
          echo "  可用内存比例: ${avail_pct}%"
        fi
      fi
      echo ""
      echo "【建议】"
      if [ "$(echo "$avail_pct < 20" | bc 2>/dev/null)" = "1" ]; then
        echo "  ⚠ 系统可用内存不足 20%，建议排查高内存占用进程或增加物理内存"
      else
        echo "  ✓ 系统内存状态正常"
      fi
      echo "============================================"
    } > "$REPORT_FILE"

    echo "{"
    echo '"success":true,'
    echo '"report_file":"'$REPORT_FILE'",'
    echo '"report_content":"'$(cat "$REPORT_FILE" | sed 's/"/\\\\"/g' | sed ':a;N;$!ba;s/\n/\\n/g')'"'
    echo "}"
    ;;

  perf)
    # 运行通用命令
    ARGS="$@"
    OUTPUT=$(timeout 10 $ARGS 2>&1)
    EXITCODE=$?
    echo "{"
    echo '"success":'$([ $EXITCODE -eq 0 ] && echo 'true' || echo 'false')','
    echo '"exit_code":'$EXITCODE','
    echo '"output":"'$(echo "$OUTPUT" | head -100 | sed 's/"/\\\\"/g' | sed ':a;N;$!ba;s/\n/\\n/g')'"'
    echo "}"
    ;;

  *)
    echo '{"success":false,"error":"用法: perf_analyze.sh cpu|disk|memory|cpuhotspot|memoryleak|strace|report|perf [args]"}'
    ;;
esac
