#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; cd "$ROOT"
PORT="${TEST_PORT:-18084}"
rm -f /tmp/OSSP_SHOULD_NOT_EXIST
./monitor_web --port "$PORT" >/tmp/ossp_security_server.log 2>&1 & S=$!
cleanup(){ kill -INT "$S" 2>/dev/null || true; wait "$S" 2>/dev/null || true; }; trap cleanup EXIT
sleep 2
CODE=$(curl -sS -o /tmp/ossp_invalid_remote.json -w '%{http_code}' -X POST "http://127.0.0.1:$PORT/api/machines/test" -H 'Content-Type: application/json' --data '{"display_name":"Bad","host":"127.0.0.1;touch /tmp/OSSP_SHOULD_NOT_EXIST","username":"student","port":22}')
test "$CODE" = 400
test ! -e /tmp/OSSP_SHOULD_NOT_EXIST
# A syntactically valid target exercises graceful SSH-unavailable/auth/connect failure. It may be 200 only if localhost SSH happens to be configured.
CODE2=$(curl -sS -o /tmp/ossp_ssh_result.json -w '%{http_code}' -X POST "http://127.0.0.1:$PORT/api/machines/test" -H 'Content-Type: application/json' --data '{"display_name":"Loopback","host":"127.0.0.1","username":"student","port":22}')
test "$CODE2" = 200 -o "$CODE2" = 502
grep -Eq '"connected":(true|false)' /tmp/ossp_ssh_result.json
echo 'security_test: PASS'
