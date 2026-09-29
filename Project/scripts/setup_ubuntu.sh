#!/usr/bin/env bash
set -euo pipefail
sudo apt update
sudo apt install -y build-essential curl openssh-client
printf '\nSetup complete. Build with: make\nRun web UI with: ./monitor_web\n'
