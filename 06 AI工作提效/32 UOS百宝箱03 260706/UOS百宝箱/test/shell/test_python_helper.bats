#!/usr/bin/env bats

setup() {
  export HELPER="$BATS_TEST_DIRNAME/../../resources/tool_helper.py"
}

@test "tool_helper.py should exist and be readable" {
  [ -f "$HELPER" ]
}

@test "tool_helper.py should have valid Python syntax" {
  run python3 -m py_compile "$HELPER"
  [ "$status" -eq 0 ]
}

@test "tool_helper.py image-resize should fail without params" {
  run python3 "$HELPER" image-resize '{}'
  [ "$status" -eq 0 ]
  echo "$output" | grep -q "success.*false\|error"
}

@test "tool_helper.py image-convert should fail without params" {
  run python3 "$HELPER" image-convert '{}'
  [ "$status" -eq 0 ]
  echo "$output" | grep -q "success.*false\|error"
}

@test "tool_helper.py ocr should fail without params" {
  run python3 "$HELPER" ocr '{}'
  [ "$status" -eq 0 ]
  echo "$output" | grep -q "success.*false\|error"
}

@test "tool_helper.py scan-effect should fail without params" {
  run python3 "$HELPER" scan-effect '{}'
  [ "$status" -eq 0 ]
  echo "$output" | grep -q "success.*false\|error"
}

@test "tool_helper.py query-cve should fail without CVE ID" {
  run python3 "$HELPER" query-cve ''
  [ "$status" -eq 0 ]
  echo "$output" | grep -q "success.*false\|error"
}

@test "tool_helper.py check-tesseract should return JSON" {
  run python3 "$HELPER" check-tesseract '{}'
  [ "$status" -eq 0 ]
  echo "$output" | grep -q "success"
}

@test "tool_helper.py should fail with unknown command" {
  run python3 "$HELPER" nonexistent-command '{}'
  [ "$status" -eq 0 ]
  echo "$output" | grep -q "未知命令\|error\|false"
}

@test "tool_helper.py should fail with no arguments" {
  run python3 "$HELPER"
  [ "$status" -eq 0 ]
  echo "$output" | grep -q "success.*false\|error\|缺少参数"
}

@test "tool_helper.py install-tesseract should return advice message" {
  run python3 "$HELPER" install-tesseract '{}'
  [ "$status" -eq 0 ]
  echo "$output" | grep -q "sudo apt-get install"
}

@test "tool_helper.py should return valid JSON for all commands" {
  for cmd in check-tesseract image-resize image-convert ocr query-cve install-tesseract; do
    run python3 "$HELPER" "$cmd" '{}'
    echo "Testing $cmd: $output"
    echo "$output" | python3 -c "import sys,json; json.loads(sys.stdin.read())" 2>/dev/null
    [ "$?" -eq 0 ]
  done
}
