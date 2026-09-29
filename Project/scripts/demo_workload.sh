#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TMP="/tmp/ossp_demo_memory_$$"
cleanup(){ kill "${CPU_PID:-}" "${SLEEP_PID:-}" "${MEM_PID:-}" 2>/dev/null || true; rm -f "$TMP"; }
trap cleanup EXIT INT TERM
cc -O2 "$ROOT/scripts/demo_memory.c" -o "$TMP"
echo "Starting safe demonstration workloads for ~6-8 seconds..."
( end=$((SECONDS+6)); while [ "$SECONDS" -lt "$end" ]; do :; done ) & CPU_PID=$!
"$TMP" & MEM_PID=$!
sleep 8 & SLEEP_PID=$!
echo "CPU PID=$CPU_PID | Memory PID=$MEM_PID | Sleep PID=$SLEEP_PID"
echo "Watch these PIDs appear as STARTED/ACTIVE and then EXITED in the dashboard."
wait "$CPU_PID" || true
wait "$MEM_PID" || true
wait "$SLEEP_PID" || true
echo "Demo workload finished and cleaned up."
