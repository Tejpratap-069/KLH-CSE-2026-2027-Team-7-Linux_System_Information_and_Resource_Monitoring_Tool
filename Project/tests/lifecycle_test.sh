#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; cd "$ROOT"
PORT="${TEST_PORT:-18083}"
./monitor_web --port "$PORT" >/tmp/ossp_lifecycle_server.log 2>&1 & S=$!
cleanup(){ kill -INT "$S" 2>/dev/null || true; wait "$S" 2>/dev/null || true; }; trap cleanup EXIT
sleep 2
sleep 3 & P=$!
sleep 2
curl -fsS "http://127.0.0.1:$PORT/api/lifecycle" >/tmp/ossp_life_started.json
python3 - "$P" <<'PY'
import json,sys
pid=int(sys.argv[1]); e=json.load(open('/tmp/ossp_life_started.json'))['events']
assert any(x['pid']==pid and x['event']=='STARTED' for x in e)
print('STARTED detected for',pid)
PY
wait "$P" || true
sleep 2
curl -fsS "http://127.0.0.1:$PORT/api/lifecycle" >/tmp/ossp_life_exited.json
python3 - "$P" <<'PY'
import json,sys
pid=int(sys.argv[1]); e=json.load(open('/tmp/ossp_life_exited.json'))['events']
assert any(x['pid']==pid and x['event']=='EXITED' for x in e)
print('EXITED detected for',pid)
PY
echo 'lifecycle_test: PASS'
