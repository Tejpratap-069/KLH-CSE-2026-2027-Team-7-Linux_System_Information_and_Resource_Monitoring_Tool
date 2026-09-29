#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; cd "$ROOT"
PORT="${TEST_PORT_QUICK:-18088}"
TMP="/tmp/ossp_quick_$$"
CFG="config/machines.conf"
BACKUP=""
if [ -f "$CFG" ]; then BACKUP="${TMP}_machines.conf"; cp "$CFG" "$BACKUP"; fi
mkdir -p "$TMP"
cat >"$TMP/ssh" <<'SH'
#!/bin/sh
printf '%s\n' "$*" >>/tmp/ossp_quick_ssh_args
cat >/dev/null
printf '%s\n' 'HOST|lab-laptop'
printf '%s\n' 'KERNEL|6.8.0-test'
printf '%s\n' 'DISTRO|Ubuntu Test Linux'
printf '%s\n' 'PROC|yes'
SH
chmod +x "$TMP/ssh"
rm -f /tmp/ossp_quick_ssh_args
PATH="$TMP:$PATH" ./monitor_web --port "$PORT" >/tmp/ossp_quick_server.log 2>&1 & S=$!
cleanup(){
  kill -INT "$S" 2>/dev/null || true; wait "$S" 2>/dev/null || true
  if [ -n "$BACKUP" ] && [ -f "$BACKUP" ]; then mv "$BACKUP" "$CFG"; else rm -f "$CFG"; fi
  rm -rf "$TMP"
}
trap cleanup EXIT
for _ in $(seq 1 30); do curl -fsS "http://127.0.0.1:$PORT/api/health" >/dev/null 2>&1 && break; sleep .2; done
BAD=$(curl -sS -o /tmp/ossp_quick_bad.json -w '%{http_code}' -X POST "http://127.0.0.1:$PORT/api/quick-connect" -H 'Content-Type: application/json' --data '{"host":"192.168.1.42;touch /tmp/nope"}')
test "$BAD" = 400
GOOD=$(curl -sS -o /tmp/ossp_quick_good.json -w '%{http_code}' -X POST "http://127.0.0.1:$PORT/api/quick-connect" -H 'Content-Type: application/json' --data '{"host":"192.168.1.42"}')
test "$GOOD" = 200
curl -fsS "http://127.0.0.1:$PORT/api/machines" >/tmp/ossp_quick_machines.json
curl -fsS "http://127.0.0.1:$PORT/" >/tmp/ossp_quick_index.html
python3 - <<'PY'
import json
q=json.load(open('/tmp/ossp_quick_good.json'))
assert q['connected'] is True and q['selected'] is True
assert q['host']=='192.168.1.42'
assert q['display_name']=='lab-laptop'
assert q['ssh_user']=='__ssh_config__'
m=json.load(open('/tmp/ossp_quick_machines.json'))
row=next(x for x in m['machines'] if x.get('host')=='192.168.1.42')
assert row['username']=='__ssh_config__'
assert m['selected_id']==row['id']
print('IP-only quick connect backend: PASS')
PY
grep -q 'IP-ONLY QUICK CONNECT' /tmp/ossp_quick_index.html
grep -q 'id="quickIp"' /tmp/ossp_quick_index.html
grep -q '192.168.1.42' /tmp/ossp_quick_ssh_args
! grep -q '@192.168.1.42' /tmp/ossp_quick_ssh_args
echo 'quick_connect_test: PASS'
