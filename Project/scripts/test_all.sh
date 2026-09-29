#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; cd "$ROOT"
for t in smoke_test api_test process_test lifecycle_test applications_test security_test machine_state_test quick_connect_test; do
  echo "== $t =="
  bash "tests/$t.sh"
done
echo 'ALL TESTS PASSED'
