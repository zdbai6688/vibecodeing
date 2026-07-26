#!/bin/bash
# 文件搜索工具

name="${1:-}"
path="${2:-$HOME}"
type="${3:-any}"
size="${4:-any}"
date="${5:-any}"

if [ -z "$name" ]; then
  echo "请指定搜索关键词"
  exit 1
fi

cmd="find \"$path\" -iname '*${name}*' -not -path '*/\.*' 2>/dev/null"

case "$type" in
  file)   cmd="$cmd -type f" ;;
  dir)    cmd="$cmd -type d" ;;
  link)   cmd="$cmd -type l" ;;
esac

case "$size" in
  small)  cmd="$cmd -size -1M" ;;
  medium) cmd="$cmd -size +1M -size -100M" ;;
  large)  cmd="$cmd -size +100M" ;;
esac

case "$date" in
  today)  cmd="$cmd -newerct '0:00' 2>/dev/null" ;;
  week)   cmd="$cmd -mtime -7" ;;
  month)  cmd="$cmd -mtime -30" ;;
esac

eval "$cmd" | head -200 | while read -r f; do
  if [ -f "$f" ]; then
    size=$(stat -c%s "$f" 2>/dev/null)
    mtime=$(stat -c%y "$f" 2>/dev/null | cut -d. -f1)
    echo "FILE|$size|$mtime|$f"
  elif [ -d "$f" ]; then
    echo "DIR|||$f"
  fi
done
