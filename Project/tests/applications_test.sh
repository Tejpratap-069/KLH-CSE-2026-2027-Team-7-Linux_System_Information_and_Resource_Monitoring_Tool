#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; cd "$ROOT"
PORT1="${TEST_PORT_APPS_LOCAL:-18086}"
PORT2="${TEST_PORT_APPS_WSL:-18087}"
S1=""; S2=""; TMP="/tmp/ossp_fake_ps_$$"
cleanup(){
  [ -z "$S1" ] || { kill -INT "$S1" 2>/dev/null || true; wait "$S1" 2>/dev/null || true; }
  [ -z "$S2" ] || { kill -INT "$S2" 2>/dev/null || true; wait "$S2" 2>/dev/null || true; }
  rm -rf "$TMP"
}
trap cleanup EXIT

./monitor_web --port "$PORT1" >/tmp/ossp_apps_local.log 2>&1 & S1=$!
for _ in $(seq 1 30); do curl -fsS "http://127.0.0.1:$PORT1/api/health" >/dev/null 2>&1 && break; sleep .2; done
curl -fsS "http://127.0.0.1:$PORT1/api/applications" >/tmp/ossp_apps_local.json
curl -fsS "http://127.0.0.1:$PORT1/" >/tmp/ossp_apps_index.html
python3 - <<'PY'
import json
j=json.load(open('/tmp/ossp_apps_local.json'))
assert 'available' in j and isinstance(j.get('applications'), list)
assert 'detection_mode' in j
print('native application endpoint: PASS')
PY
grep -q 'OPEN APPLICATIONS' /tmp/ossp_apps_index.html
kill -INT "$S1" 2>/dev/null || true; wait "$S1" 2>/dev/null || true; S1=""

# Deterministic WSL-integration parser test. This temporary executable is only a test harness;
# production runtime still invokes the real Windows PowerShell through WSL interop.
mkdir -p "$TMP"
cat >"$TMP/powershell.exe" <<'PS'
#!/bin/sh
printf '%s\n' 'APP|4321|chrome|536870912|12.500|ChatGPT - Google Chrome'
printf '%s\n' 'APP|7777|Code|268435456|3.250|Visual Studio Code'
PS
chmod +x "$TMP/powershell.exe"
WSL_DISTRO_NAME=Ubuntu PATH="$TMP:$PATH" ./monitor_web --port "$PORT2" >/tmp/ossp_apps_wsl.log 2>&1 & S2=$!
for _ in $(seq 1 30); do curl -fsS "http://127.0.0.1:$PORT2/api/health" >/dev/null 2>&1 && break; sleep .2; done
curl -fsS "http://127.0.0.1:$PORT2/api/applications" >/tmp/ossp_apps_wsl.json
python3 - <<'PY'
import json
j=json.load(open('/tmp/ossp_apps_wsl.json'))
assert j['source']=='windows-host'
rows=j['applications']
chrome=next(x for x in rows if x['name'].lower()=='chrome')
assert chrome['application']=='Google Chrome'
assert chrome['kind']=='browser'
assert chrome['title']=='ChatGPT - Google Chrome'
assert chrome['ram_mb']==512.0
assert isinstance(chrome['cpu_percent'], (int,float))
print('WSL Windows visible-app parser: PASS')
PY
kill -INT "$S2" 2>/dev/null || true; wait "$S2" 2>/dev/null || true; S2=""
if command -v node >/dev/null 2>&1; then node --check frontend/app.js; fi
echo 'applications_test: PASS'
