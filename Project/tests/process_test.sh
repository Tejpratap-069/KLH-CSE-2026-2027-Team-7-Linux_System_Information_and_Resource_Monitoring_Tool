#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; cd "$ROOT"
PORT="${TEST_PORT:-18082}"
./monitor_web --port "$PORT" >/tmp/ossp_proc_server.log 2>&1 & S=$!
cleanup(){ kill -INT "$S" 2>/dev/null || true; wait "$S" 2>/dev/null || true; }; trap cleanup EXIT
sleep 2
sleep 6 & P=$!
sleep 2
curl -fsS "http://127.0.0.1:$PORT/api/processes" >/tmp/ossp_proc_rows.json
python3 - "$P" <<'PY'
import json,sys
pid=int(sys.argv[1]); rows=json.load(open('/tmp/ossp_proc_rows.json'))['processes']
row=next((x for x in rows if x['pid']==pid),None)
assert row, f'PID {pid} not found'
assert row['ppid'] > 0
assert row['ram_mb'] >= 0
print('live process row:', row['pid'], row['name'], row['state'])
PY
curl -fsS "http://127.0.0.1:$PORT/api/process/$P" >/tmp/ossp_proc_inspector.json
grep -q '"memory"' /tmp/ossp_proc_inspector.json
grep -q '"io"' /tmp/ossp_proc_inspector.json
wait "$P" || true
# Real successive-sample process CPU calculation: a bounded busy shell should consume a core.
bash -c 'end=$((SECONDS+4)); while [ $SECONDS -lt $end ]; do :; done' & CPU=$!
sleep 2
curl -fsS "http://127.0.0.1:$PORT/api/processes" >/tmp/ossp_cpu_rows.json
python3 - "$CPU" <<'PY'
import json,sys
pid=int(sys.argv[1]); rows=json.load(open('/tmp/ossp_cpu_rows.json'))['processes']
row=next((x for x in rows if x['pid']==pid),None)
assert row and row['cpu_percent'] > 1.0, (pid,row)
print('real process CPU measured:', row['cpu_percent'])
PY
wait "$CPU" || true
echo 'process_test: PASS'
