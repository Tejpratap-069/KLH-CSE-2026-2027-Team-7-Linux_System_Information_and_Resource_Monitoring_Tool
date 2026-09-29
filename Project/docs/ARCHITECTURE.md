# Architecture

## 1. User-space application layer

Both binaries are ordinary user-space programs. `system_monitor` presents terminal output. `monitor_web` starts the same monitoring core, a C socket HTTP server, JSON API routing and the static frontend.

## 2. Linux information sources

Local collection uses `/proc/stat`, `/proc/cpuinfo`, `/proc/meminfo`, `/proc/uptime`, `/proc/loadavg`, `/proc/<PID>/stat`, `/proc/<PID>/status`, `/proc/<PID>/cmdline`, `/proc/<PID>/io`, `/proc/<PID>/maps`, `/proc/<PID>/fd`, `/sys/class/net/...`, `statvfs()`, `uname()`, `gethostname()` and `getifaddrs()`.

The C program performs bounded reads and directory scans directly. It does not scrape `top` or `htop`.

## 3. Concurrency

The monitor has a long-lived POSIX collector thread. The web server creates detached worker threads for accepted HTTP clients. Shared snapshot, selected-machine state, lifecycle history and tracked-process state are protected by a `pthread_mutex_t`.

## 4. HTTP

`socket() → setsockopt() → bind() → listen() → accept()` is used directly. The server intentionally implements only the routes needed by the project. Request/header/body limits are enforced. Static files are served from `frontend/`.

## 5. Process identity and lifecycle

A Linux PID can be reused. Therefore a process instance is identified using `PID + /proc/<PID>/stat starttime`. A new pair is STARTED; an existing pair remains ACTIVE; a previously tracked pair that disappears is EXITED.

## 6. Open Applications layer

The low-level Linux process table and the user-facing Open Applications view are intentionally separate.

- **WSL2:** the backend detects WSL and uses `fork()` + `exec*()` to launch a fixed Windows PowerShell `Get-Process` query through WSL interop. Only native Windows processes with visible top-level windows are returned. Browser-controlled text is never inserted into this PowerShell command.
- **Native Linux:** the backend reads `/proc/<PID>/environ` for `DISPLAY` / `WAYLAND_DISPLAY` and confirms GUI client evidence in `/proc/<PID>/maps` using X11/XCB, Wayland, GTK/GDK or Qt GUI libraries.
- **Remote Linux:** a fixed SSH-side read-only script performs the same `/proc` checks and correlates PIDs with the existing remote process snapshot.

The frontend presents application name, type, PID, CPU, RAM, process count and, where available, the visible window title. A browser window title can identify the visible web-app/page title, but the project does not claim access to every tab or URL.

## 7. Remote architecture

The main laptop never installs the monitoring project on the remote laptop. When a remote machine is selected, the collector forks and executes the local OpenSSH client with a fixed option/argument list. A fixed read-only collection script is sent to remote `sh -s` through stdin. It reads remote `/proc` and `/sys` data and returns normalized records. The C backend computes interval rates/CPU percentages and places them into the same `MonitorSnapshot` shape used by the local dashboard.

Browser-controlled hostname, username and port values are validated; no browser text is appended to a local shell command. The private-key path comes from the backend environment (`MONITOR_SSH_IDENTITY`) and is never returned to the browser.

## 8. Shutdown

`SIGINT` and `SIGTERM` are installed with `sigaction()`. The main stop flag causes the poll/accept loop to end and the monitor thread to join. Open listening descriptors are closed and process memory is released by normal program exit.
