#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
test -x ./system_monitor
test -x ./monitor_web
./system_monitor --once >/tmp/ossp_terminal_once.txt
grep -q 'Linux System Information and Resource Monitoring Tool' /tmp/ossp_terminal_once.txt
./system_monitor --process-demo >/tmp/ossp_demo.json
grep -q '"child_exit_status":0' /tmp/ossp_demo.json
echo 'smoke_test: PASS'
