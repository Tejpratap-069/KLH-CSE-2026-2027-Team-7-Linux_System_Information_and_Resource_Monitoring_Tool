# Linux System Information and Resource Monitoring Tool

**Course:** Operating Systems and Systems Programming — 25CS2104E  
**Primary language:** C11 / POSIX  
**Frontend:** HTML5, CSS3, Vanilla JavaScript  
**Platforms:** Native Linux/Ubuntu and Ubuntu under WSL2

This project is a full Linux systems-programming monitor. It reads live kernel/user-space interfaces rather than inventing telemetry. CPU, memory, process, disk, network and lifecycle data come from `/proc`, `/sys`, POSIX/Linux APIs, and filesystem interfaces. A C HTTP server exposes JSON APIs and serves a dependency-free dashboard. A terminal application remains available over the same monitoring core.

The implementation follows the approved project direction: a C/Ubuntu monitoring utility demonstrating user/kernel space, system calls, process lifecycle, file descriptors/file I/O, memory management, POSIX threads, synchronization and signal handling. The web and agentless SSH layers are presentation/extension layers over that core.

## Architecture

```text
                    ┌─────────────────────────┐
Browser ──HTTP─────►│ C socket HTTP server    │
                    │ + API router            │
                    └──────────┬──────────────┘
                               │ mutex-protected snapshots
                    ┌──────────▼──────────────┐
                    │ Monitor worker thread   │
                    └───────┬────────┬────────┘
                            │        │
             LOCAL          │        │ REMOTE (selected machine)
          /proc + /sys ◄────┘        └──► fork/exec ssh ──► authorized Linux host
               │                               │
    open/read/close, statvfs,             ephemeral fixed script
    opendir/readdir, getifaddrs           reads remote /proc + /sys
```

The HTTP server also uses short-lived POSIX worker threads for concurrent client requests. Shared monitor snapshots and lifecycle history are protected with a `pthread_mutex_t`.

## What is monitored

- CPU utilization from successive `/proc/stat` samples, CPU model and core count from `/proc/cpuinfo`, and 1/5/15-minute load averages.
- RAM and swap from `/proc/meminfo`.
- Root filesystem usage through `statvfs()`.
- Uptime from `/proc/uptime`.
- Hostname, distribution and kernel information.
- Network interfaces from `/sys/class/net`, including link state, byte counters, IPv4 addresses and RX/TX rates.
- Every visible process: PID, PPID, user/UID, state, name, CPU %, RAM MB/%, thread count, command line and start time.
- Per-PID inspector: executable, working directory, virtual/resident memory fields, I/O counters, open FD count and optional memory maps.
- Bounded process lifecycle: STARTED, ACTIVE and EXITED, identified by **PID + process start ticks**, not PID alone.
- **Open Applications** view: on WSL2 it uses controlled Windows PowerShell interop to enumerate real native Windows processes that own visible top-level windows, including Chrome/Edge/Firefox/VS Code and their PID, CPU %, RAM and visible window title. Browser window titles are surfaced as a useful **web-app/page-title hint**. On native/remote Linux it uses `/proc/<PID>/environ` plus GUI-library evidence from `/proc/<PID>/maps` to identify GUI-session applications without fabricated rows.

## Process CPU calculation

Linux stores process time in jiffies. For two consecutive samples:

```text
process_delta = Δ(utime + stime)
system_delta  = Δ(total jiffies from aggregate /proc/stat cpu line)
process_CPU_%  = (process_delta / system_delta) × online_CPU_count × 100
```

The CPU-count multiplication converts the aggregate-all-CPUs denominator to the familiar convention where one fully occupied core is roughly 100%. The first sample has no previous interval, so it correctly starts at 0 until another real sample exists. A process is matched to its previous sample by PID **and** `/proc/<PID>/stat` start time, preventing PID reuse from corrupting the calculation.

## Process lifecycle logic

The collector keeps a bounded tracked-process table. Each instance is keyed by `(PID, start_ticks)`. On every snapshot:

- new key → `STARTED`;
- still present → resource peaks are updated and periodic `ACTIVE` events are emitted;
- previously tracked key missing → `EXITED`.

Each record keeps first/last seen timestamps, lifetime, latest CPU/RAM, peak CPU/RAM and final state. The history is bounded to prevent unbounded memory growth.

## Prerequisites

Ubuntu/Debian:

```bash
sudo apt update
sudo apt install build-essential curl openssh-client -y
```

Or run:

```bash
./scripts/setup_ubuntu.sh
```

No npm, Node.js, Flask, Django, Spring Boot, database, CDN or JavaScript package install is required.

## Build

```bash
make clean
make
```

This builds:

```text
system_monitor
monitor_web
```

Compiler flags include `-std=c11 -Wall -Wextra -Wpedantic -O2 -pthread`.

Useful targets:

```bash
make clean
make test
make web-run
make debug
```

`make debug` builds with AddressSanitizer + UndefinedBehaviorSanitizer.

## Run the web dashboard

```bash
./monitor_web
```

Open:

```text
http://localhost:8080
```

Alternative port:

```bash
./monitor_web --port 8081
```

Trusted LAN only:

```bash
./monitor_web --bind-lan
```

The default binding is localhost. LAN binding exposes the dashboard to other machines on the network and should only be used intentionally on a trusted network.

## Run the terminal monitor

```bash
./system_monitor
./system_monitor --once
./system_monitor --interval 2
./system_monitor --process-demo
```

Ctrl+C is handled with `sigaction()` and exits cleanly.

## IPC demonstration

The dashboard button **Run Process / IPC Demo** and `./system_monitor --process-demo` perform a real fixed demonstration:

1. create an anonymous `pipe()`;
2. `fork()` a child;
3. child reads `/proc/uptime`;
4. child writes the result to the pipe;
5. parent reads the message;
6. parent calls `waitpid()` and reports child exit status.

No browser-supplied command is executed.

## Safe demonstration workload

```bash
./scripts/demo_workload.sh
```

It compiles a tiny local helper that allocates/touches **32 MiB** for six seconds, creates a short bounded CPU loop and a sleep process, then cleans all temporary resources. Use the dashboard to show the PIDs starting, becoming active and exiting.

## Remote laptop monitoring — IP-only dashboard flow

Remote monitoring remains **agentless and authorized over OpenSSH**. The remote laptop needs Linux, an OpenSSH server, and one normal SSH authorization. It does **not** need this project or an agent installed.

### One-time remote authorization

On the remote Linux laptop:

```bash
sudo apt update
sudo apt install openssh-server -y
sudo systemctl enable --now ssh
```

On the main laptop/WSL environment:

```bash
sudo apt install openssh-client -y
ssh-keygen -t ed25519
ssh-copy-id user@REMOTE_IP
ssh user@REMOTE_IP
```

The final manual SSH command must work without a password prompt. This authorization is an operating-system security requirement; knowing an IP address alone must not grant access to another machine.

### Daily dashboard use: IP only

After authorization, the redesigned **Machines → IP-only Quick Connect** page asks for one visible field only:

```text
IPv4 address: 192.168.1.42
[ Connect ]
```

The browser sends only the IPv4 address to `POST /api/quick-connect`. The C backend validates it, uses OpenSSH's normal account/key resolution, tests `/proc`, saves the endpoint if necessary, selects it, and switches the entire dashboard to the remote source. OpenSSH can obtain the account from `~/.ssh/config` or its normal local-account default. If needed, override that backend-only default once with:

```bash
export MONITOR_SSH_USER="remote_linux_username"
export MONITOR_SSH_IDENTITY="$HOME/.ssh/id_ed25519"   # optional; default SSH keys work too
./monitor_web
```

No username, SSH port, password, or private key is requested by the IP-only browser form. Port 22 is used by default; `MONITOR_SSH_PORT` can override it on the backend.

The backend calls `ssh` with an argument array using `execvp()` and sends only fixed monitoring scripts over SSH stdin. It never builds a local `sh -c` command from the entered IP and exposes no arbitrary remote shell through the browser.

Read `docs/REMOTE_MONITORING.md` for the two-laptop validation procedure.

## WSL2

When run inside Ubuntu/WSL2, the Linux monitoring core still correctly treats `/proc` and `/sys` as the **WSL Linux environment**. In addition, the web dashboard's **Open Applications** section uses WSL interop to invoke a fixed, non-user-controlled Windows PowerShell query that enumerates native Windows processes with visible top-level windows. This allows Chrome, Edge, Firefox, VS Code, File Explorer and similar Windows applications to appear even though they are not Linux `/proc` processes.

For browsers, the dashboard displays the visible browser window title (for example `ChatGPT - Google Chrome`) and highlights the page/web-app title. This is **not** browser-history or all-tab inspection: it does not expose every tab or URL. If WSL interop/PowerShell is unavailable, the dashboard clearly falls back to WSL Linux GUI-session detection instead of inventing Windows rows. The Windows browser can normally open `http://localhost:8080` while the backend runs in WSL2. See `docs/WSL_NOTES.md` and `docs/OPEN_APPLICATIONS.md`.

## API

Core routes:

```text
GET    /api/health
GET    /api/machines
POST   /api/quick-connect
POST   /api/machines/test
POST   /api/machines
DELETE /api/machines/<id>
POST   /api/select-machine
GET    /api/system
GET    /api/processes
GET    /api/applications
GET    /api/process/<pid>
GET    /api/process/<pid>/maps
GET    /api/lifecycle
GET    /api/network
POST   /api/process-demo
```

See `docs/API.md`.

## Testing

```bash
make test
```

The supplied suite verifies build artifacts, terminal collection, IPC, `/api/health`, numeric real system values, live PIDs, PID inspector, real per-process CPU deltas, process lifecycle start/exit detection, the Open Applications API/UI hook, deterministic WSL Windows-app parser behavior, frontend serving, malformed-route survival, remote-input validation, command-injection rejection, and local/remote source-state isolation. See `docs/TEST_RESULTS.md` for the exact executed validation and physical-SSH limitation.

## Security boundaries

This is an authorized monitoring tool, not a stealth/admin tool. It does not implement credential capture, privilege escalation, random-host scanning, exploitation, hidden persistence or a browser-exposed remote shell. Remote access only works after ordinary SSH authorization.

Process visibility is limited by Linux permissions. Privileged process information may report `Permission unavailable` rather than crashing.

## Troubleshooting

**Port 8080 already in use**

```bash
./monitor_web --port 8081
```

**SSH client is not installed on the main laptop**

```bash
sudo apt install openssh-client -y
```

**Remote authentication fails**

Manually run `ssh user@REMOTE_HOST`. Fix key authorization first. The monitor uses `BatchMode=yes`, so it intentionally does not fall back to password prompting.

**Remote host unreachable / timeout**

Check IP/hostname, same network/VPN as appropriate, SSH service, firewall and port 22 (or the configured port).

**Process disappeared during inspection**

This is normal in a live OS. The process may have exited between table refresh and inspector read; the API returns an unavailable status instead of dereferencing stale data.

## Screenshots

The project is runnable without bundled screenshots. The build container could not complete a headless Chromium capture; `docs/SCREENSHOTS.md` records the honest screenshot checklist. Recommended faculty screenshots after launching locally:

1. machine header + metric cards;
2. live resource graph;
3. process table with a high-CPU demo PID;
4. process inspector and `/proc/<PID>/maps` expansion;
5. lifecycle showing STARTED and EXITED for the demo workload;
6. successful SSH Test Connection on a second authorized Linux laptop.
