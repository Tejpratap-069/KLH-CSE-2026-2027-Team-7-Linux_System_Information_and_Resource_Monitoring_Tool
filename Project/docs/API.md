# HTTP API

Default base URL: `http://127.0.0.1:8080`

Responses are JSON except static frontend files.

## `GET /api/health`

Simple service health response.

## `GET /api/machines`

Returns configured machines and `selected_id`. The built-in local machine cannot be deleted.

## `POST /api/quick-connect`

IP-only remote connection used by the redesigned dashboard.

```json
{"host":"192.168.1.42"}
```

The host must be a valid IPv4 address. The browser does not send an SSH username, password, port, command, or private key. The backend uses normal OpenSSH configuration/defaults (or the optional backend environment variables `MONITOR_SSH_USER`, `MONITOR_SSH_PORT`, and `MONITOR_SSH_IDENTITY`), tests the authorized SSH connection, automatically saves the endpoint when new, selects it, and returns the selected machine identity.

Invalid/injection-like host strings are rejected before `ssh` is executed.

## `POST /api/machines/test`

Body:

```json
{"display_name":"Lab PC","host":"192.168.1.42","username":"student","port":22}
```

Tests authorized key-based SSH. No password field exists.

## `POST /api/machines`

Saves a validated remote machine in `config/machines.conf`.

## `DELETE /api/machines/<id>`

Deletes a saved remote machine. `local` is protected.

## `POST /api/select-machine`

```json
{"id":"lab-pc"}
```

Switches the collector source. When remote is selected, subsequent system/process/lifecycle snapshots are remote-only.

## `GET /api/system`

Returns source, machine identity, timestamp, freshness, host metadata, CPU/load, memory, swap, disk, process summary and network interfaces/rates.

## `GET /api/processes`

Returns real process rows including PID, PPID, UID/user, state, name, CPU %, RAM MB/%, threads, command line and process start time.

## `GET /api/applications`

Returns the current **Open Applications** view.

- On WSL2: the backend uses a fixed Windows PowerShell `Get-Process` query through WSL interop and returns native Windows processes with visible top-level windows, including PID, friendly application name, type, CPU %, RAM MB and visible window title. Browser window titles can be used as a current page/web-app title hint.
- On native Linux: it correlates the current process snapshot with `/proc/<PID>/environ` (`DISPLAY` / `WAYLAND_DISPLAY`) and GUI-library evidence in `/proc/<PID>/maps`.
- On a remote Linux machine: the same Linux GUI evidence is gathered over authorized SSH and correlated with the selected remote process snapshot.

The endpoint never fabricates an application row. If a platform-specific collector is unavailable, it reports the limitation/fallback in `available`, `platform`, `detection_mode`, `note`, and `error` fields.

## `GET /api/process/<pid>`

Detailed local/remote process inspector. Local mode includes current CPU/RAM from the prepared snapshot plus direct live `/proc` detail fields.

## `GET /api/process/<pid>/maps`

Returns an optional text representation of the process memory map. Local reads are capped to protect response size.

## `GET /api/lifecycle`

Returns bounded newest-first STARTED/ACTIVE/EXITED history for the selected machine.

## `GET /api/network`

Returns the current system snapshot (including detailed network block) for a lightweight network-focused consumer.

## `POST /api/process-demo`

Runs the fixed local fork/pipe/waitpid educational demonstration. It does not accept a command from the browser.

## Limits / behavior

- request buffer: 64 KiB
- JSON body limit: 32 KiB
- URL limit: 2048 characters
- static file cap: 8 MiB
- remote SSH output cap: 6 MiB
- remote connection timeouts are bounded
- unknown API routes return 404
