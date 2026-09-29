#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; cd "$ROOT"
PORT="${TEST_PORT:-18085}"
CFG="config/machines.conf"
BACKUP="/tmp/ossp_machines_backup_$$"
HAD_CFG=0
if [ -f "$CFG" ]; then cp "$CFG" "$BACKUP"; HAD_CFG=1; fi
restore_cfg(){ if [ "$HAD_CFG" -eq 1 ]; then mv -f "$BACKUP" "$CFG"; else rm -f "$CFG" "$BACKUP"; fi; }
./monitor_web --port "$PORT" >/tmp/ossp_machine_state.log 2>&1 & S=$!
cleanup(){ kill -INT "$S" 2>/dev/null || true; wait "$S" 2>/dev/null || true; restore_cfg; }
trap cleanup EXIT
for _ in $(seq 1 40); do curl -fsS "http://127.0.0.1:$PORT/api/health" >/dev/null 2>&1 && break; sleep .2; done
curl -fsS -X POST "http://127.0.0.1:$PORT/api/machines" -H 'Content-Type: application/json' --data '{"display_name":"Loopback Test","host":"127.0.0.1","username":"student","port":22}' >/tmp/ossp_machine_add.json
RID=$(python3 - <<'PY'
import json
print(json.load(open('/tmp/ossp_machine_add.json'))['id'])
PY
)
curl -fsS -X POST "http://127.0.0.1:$PORT/api/select-machine" -H 'Content-Type: application/json' --data "{\"id\":\"$RID\"}" >/tmp/ossp_select_remote.json
sleep 2
CODE=$(curl -sS -o /tmp/ossp_remote_state.json -w '%{http_code}' "http://127.0.0.1:$PORT/api/system")
if [ "$CODE" = 200 ]; then
  python3 - <<'PY'
import json
s=json.load(open('/tmp/ossp_remote_state.json'))
assert s['source']=='remote', s
print('remote selection produced REMOTE snapshot')
PY
else
  test "$CODE" = 503
  ! grep -q '"source":"local"' /tmp/ossp_remote_state.json
  echo 'remote unavailable: correctly returned unavailable instead of stale local data'
fi
curl -fsS -X POST "http://127.0.0.1:$PORT/api/select-machine" -H 'Content-Type: application/json' --data '{"id":"local"}' >/tmp/ossp_select_local.json
for _ in $(seq 1 40); do
  CODE2=$(curl -sS -o /tmp/ossp_local_state.json -w '%{http_code}' "http://127.0.0.1:$PORT/api/system")
  if [ "$CODE2" = 200 ] && grep -q '"source":"local"' /tmp/ossp_local_state.json; then break; fi
  sleep .2
done
test "$CODE2" = 200
grep -q '"source":"local"' /tmp/ossp_local_state.json
curl -fsS -X DELETE "http://127.0.0.1:$PORT/api/machines/$RID" >/tmp/ossp_machine_delete.json
grep -q '"deleted":true' /tmp/ossp_machine_delete.json
echo 'machine_state_test: PASS'
