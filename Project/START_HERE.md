# START HERE

## Fastest way to run in Ubuntu / WSL2

```bash
cd Linux_System_Information_and_Resource_Monitoring_Tool
make
./monitor_web
```

Open **http://localhost:8080** in Chrome/Edge.

For the terminal version:

```bash
./system_monitor
./system_monitor --once
./system_monitor --interval 2
./system_monitor --process-demo
```

If port 8080 is busy:

```bash
./monitor_web --port 8081
```

For the safe faculty demonstration workload:

```bash
./scripts/demo_workload.sh
```

The web server binds to `127.0.0.1` by default. `--bind-lan` is available only for an intentional trusted-LAN demo.

For a second authorized Linux laptop, complete the SSH key authorization once, then use the new **Machines → IP-only Quick Connect** page. The web form asks for the IPv4 address only. Read `docs/REMOTE_MONITORING.md` for the one-time authorization and two-laptop test.

## Open applications

The dashboard now has **Open Applications**. On WSL2 it can show real native Windows visible apps (Chrome/Edge/Firefox/VS Code, etc.) through a fixed PowerShell bridge. Browser window titles are shown as web-app/page-title hints. See `docs/OPEN_APPLICATIONS.md`.

## New page-style dashboard

The frontend is now organized like an observability product with a persistent left sidebar:

- **Overview** — machine identity, live CPU/RAM/disk/process/network cards, graph and machine details.
- **Processes** — Open Applications, browser/window hints, Linux process table and PID inspector.
- **Lifecycle** — STARTED / ACTIVE / EXITED history.
- **Network** — interface-level `/sys` telemetry.
- **Systems lab** — real fork/pipe/waitpid demo and OSSP concept mapping.
- **Machines** — IP-only remote connection and saved-machine switching/removal.

The monitored-machine selection is global: selecting a remote machine changes every monitoring page to that source.
