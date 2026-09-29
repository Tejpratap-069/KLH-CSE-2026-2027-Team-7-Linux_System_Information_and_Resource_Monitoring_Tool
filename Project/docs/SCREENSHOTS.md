# Screenshot Checklist

The build environment could serve and test the dashboard but could not complete a Chromium headless screenshot because of its container DBus/zygote environment. These are intentional screenshot placeholders/checkpoints for your final faculty record; do not replace them with simulated telemetry.

1. **Local overview** — machine header showing `LOCAL — LIVE`, Linux distribution, kernel, uptime and metric cards.
2. **Live graph** — CPU/RAM graph after at least 10 seconds of real sampling.
3. **Process table** — PID/PPID/user/state/process/CPU/RAM/threads/start-time columns with a demo PID visible.
4. **Process inspector** — click a normal user PID and show memory, I/O, executable, working directory and open-FD count.
5. **Memory maps** — expand the inspector's `/proc/<PID>/maps` view.
6. **Lifecycle** — run `./scripts/demo_workload.sh` and capture STARTED/ACTIVE/EXITED events.
7. **IPC demo** — capture parent PID, child PID, transferred `/proc/uptime` data and child exit status.
8. **Remote test connection** — on two authorized Linux laptops, capture CONNECTED + remote hostname/kernel/distribution.
9. **Remote live dashboard** — capture `REMOTE — LIVE` and a process that was launched on the remote laptop.
10. **Disconnected behavior** — optionally demonstrate that a disconnected remote machine says unavailable/disconnected instead of showing local values.
