#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; cd "$ROOT"
PORT="${TEST_PORT:-18081}"
./monitor_web --port "$PORT" >/tmp/ossp_api_server.log 2>&1 & S=$!
cleanup(){ kill -INT "$S" 2>/dev/null || true; wait "$S" 2>/dev/null || true; }; trap cleanup EXIT
for _ in $(seq 1 30); do curl -fsS "http://127.0.0.1:$PORT/api/health" >/tmp/ossp_health.json 2>/dev/null && break; sleep .2; done
grep -q '"ok":true' /tmp/ossp_health.json
curl -fsS "http://127.0.0.1:$PORT/api/system" >/tmp/ossp_system.json
curl -fsS "http://127.0.0.1:$PORT/api/processes" >/tmp/ossp_processes.json
curl -fsS "http://127.0.0.1:$PORT/" >/tmp/ossp_index.html
grep -q 'Linux System Information' /tmp/ossp_index.html
python3 - <<'PY'
import json
s=json.load(open('/tmp/ossp_system.json'))
p=json.load(open('/tmp/ossp_processes.json'))
assert isinstance(s['cpu_percent'], (int,float))
assert isinstance(s['memory']['percent'], (int,float))
assert isinstance(s['disk']['percent'], (int,float))
assert p['processes'] and all(isinstance(x['pid'], int) for x in p['processes'])
print('api numeric + real PID checks: PASS')
PY
PID=$(python3 - <<'PY'
import json
print(json.load(open('/tmp/ossp_processes.json'))['processes'][0]['pid'])
PY
)
curl -fsS "http://127.0.0.1:$PORT/api/process/$PID" >/tmp/ossp_inspect.json
grep -q '"pid"' /tmp/ossp_inspect.json
# Unknown/malformed API route must not crash.
CODE=$(curl -sS -o /tmp/ossp_404.json -w '%{http_code}' "http://127.0.0.1:$PORT/api/does-not-exist")
test "$CODE" = 404
curl -fsS "http://127.0.0.1:$PORT/api/health" >/dev/null
echo 'api_test: PASS'
