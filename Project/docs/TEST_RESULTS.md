# Final Build and Test Results

This file records tests actually executed in the build environment. It deliberately separates verified behavior from checks that require a second physical Linux laptop.

## Build environment

- Distribution: Debian GNU/Linux 13 (trixie)
- Kernel: Linux 6.18.44 x86_64
- Compiler: GCC 14.2.0
- Build flags: `-std=c11 -Wall -Wextra -Wpedantic -O2 -pthread`
- OpenSSH client in this build container: unavailable
- Chromium binary: present, but headless screenshot mode did not complete in this container because its DBus/zygote environment is unavailable

## Release build

Executed:

```bash
make clean
make
```

Result: both `system_monitor` and `monitor_web` built successfully with no GCC warnings from the configured `-Wall -Wextra -Wpedantic` build.

## Automated suite

Executed:

```bash
make test
```

Verified by the supplied scripts:

- both binaries exist and execute;
- terminal `--once` obtains real Linux data;
- process/IPC demo completes;
- `/api/health` succeeds;
- `/api/system` returns numeric live CPU/RAM/disk values;
- `/api/processes` returns real PIDs;
- a normal PID can be inspected;
- a real temporary process is seen as `STARTED` and then `EXITED`;
- a bounded busy process produces a real non-zero per-process CPU percentage (about one fully occupied core in this environment);
- static frontend is served by the C server;
- `/api/applications` returns a real, non-fabricated Open Applications payload for the active source;
- the Open Applications HTML section is present;
- a deterministic WSL integration harness verifies parsing of Windows visible-application data, friendly Chrome naming, browser classification, real numeric CPU/RAM fields, and visible browser window-title transport; the harness is test-only and is not used by production runtime;
- unknown API routes do not crash the server;
- SSH host input containing shell metacharacters is rejected before execution;
- no injected `/tmp/OSSP_SHOULD_NOT_EXIST` file is created;
- a syntactically valid remote target fails gracefully when SSH is unavailable/unconfigured;
- after selecting a remote machine with no obtainable remote data, `/api/system` returns unavailable instead of leaking the previous local snapshot;
- switching back to `local` resumes a fresh local-source snapshot;
- the new `/api/quick-connect` rejects non-IPv4/injection-like input before SSH execution;
- an isolated fake-OpenSSH harness verifies that a valid IP-only request can test, save, select, and switch to a remote machine without any username/password/port field in the browser;
- the default IP-only path invokes OpenSSH with the host alone (no forced `user@host`), allowing normal `~/.ssh/config` account/key resolution;
- the redesigned frontend contains the page-based sidebar and the IP-only Quick Connect control.

Final automated result: **ALL TESTS PASSED**.

## Additional manual protocol/API checks

The server was launched on an alternate test port and the following were executed:

- sent raw malformed bytes `GARBAGE\r\n\r\n`: server returned `HTTP/1.1 400 Bad Request`;
- called `/api/health` immediately afterward: server still responded normally;
- inspected the running `monitor_web` PID;
- fetched `/api/process/<pid>/maps` and received a real non-empty process map;
- started a second `monitor_web` on the same port: it exited non-zero with a useful `Address already in use` message and suggested another port;
- sent SIGINT/Ctrl+C equivalent to the primary server: it printed `monitor_web shut down cleanly.`;
- ran the terminal monitor continuously, sent SIGINT, and verified `Stopped cleanly.`;
- ran `scripts/demo_workload.sh`: the CPU, 32 MiB memory, and sleep demo workloads started, printed their real PIDs, completed, and cleaned up;
- ran `node --check frontend/app.js`: JavaScript syntax check passed.

## Sanitizer verification

Executed a `make debug` build using AddressSanitizer and UndefinedBehaviorSanitizer, then exercised:

- `system_monitor --once`;
- terminal process/IPC demo;
- web server startup;
- `/api/system`;
- `/api/processes`;
- `/api/applications`;
- PID inspector;
- process maps endpoint;
- web process/IPC demo;
- clean SIGINT shutdown.

Final sanitizer result: **PASS** with no AddressSanitizer, LeakSanitizer, or UndefinedBehaviorSanitizer diagnostics on those paths. After the IP-only connection upgrade, `tests/quick_connect_test.sh` was also executed against an ASan/UBSan build and passed.

## Frontend visual-check limitation

The HTML/CSS/JavaScript frontend was served successfully and its API integration/static assets were tested. A real Chromium executable is installed in the build container, but repeated headless screenshot attempts timed out with container DBus/zygote errors, so no fabricated screenshot is included. Use the screenshot checklist in `docs/SCREENSHOTS.md` after launching in Chrome/Edge on Ubuntu/WSL.

## Remote SSH test limitation

A real second physical Linux computer was not available, and this build container does not contain the `ssh` client. Therefore a successful physical SSH monitoring session is **not claimed**. The following remote aspects were still verified locally:

- strict host/user/port validation;
- command-injection input rejection;
- fixed SSH argv design (no browser-built `sh -c`);
- fixed remote collection script in source;
- `BatchMode=yes`/timeout configuration;
- graceful SSH-unavailable response;
- source switching discards stale local snapshots instead of presenting them as remote data.

Complete two-laptop validation steps are in `docs/REMOTE_MONITORING.md` and must be run on the actual authorized remote laptop before a faculty demonstration of that feature.

## WSL Windows Open Applications validation limitation

The build container is native Linux rather than Windows/WSL and does not contain Windows PowerShell. Therefore a live Windows-host application enumeration is **not claimed** here. The WSL-specific parser/HTTP path was exercised with a deterministic temporary `powershell.exe` test harness that emits the same pipe-delimited record format expected from the fixed production command. On the user's actual Windows 11 + WSL2 machine, final confirmation consists of launching Chrome/Edge/VS Code on Windows and checking that the Open Applications panel displays their real Windows PID, RAM/CPU, and main-window title. No test-only executable is included in the packaged runtime.

