# WSL2 Notes

## What `/proc` and `/sys` represent

If `monitor_web` runs inside Ubuntu/WSL2, Linux `/proc` and `/sys` still represent the WSL2 Linux kernel/environment and the Linux processes running inside it. They must not be described as Windows Task Manager data.

The project now has a **separate Open Applications bridge** for WSL2. It does not replace the Linux monitor. It uses WSL interop to invoke a fixed Windows PowerShell `Get-Process` query and reads only processes that own a visible top-level Windows window (`MainWindowHandle != 0`). This makes real native Windows applications such as Chrome, Edge, Firefox, VS Code, File Explorer, Spotify and similar programs visible in the dashboard.

## What the Open Applications panel shows

For each visible Windows application the dashboard can show:

- application/process name;
- Windows PID;
- approximate CPU percentage measured from two real process CPU-time samples;
- working-set RAM in MB;
- visible main-window title.

For a browser, the window title often contains the current visible page title, for example `ChatGPT - Google Chrome`. The UI highlights that as a **browser / web-app hint**. This does **not** mean the backend can see every tab, browsing history, hidden/incognito tabs, or the exact URL. No fake tab data is generated.

If PowerShell/WSL interop cannot be used, the API returns a clear fallback and shows WSL Linux GUI-session processes instead.

## Recommended Windows 11 workflow

1. Open the project folder in VS Code using Ubuntu/WSL.
2. In the Ubuntu terminal:

```bash
make
./monitor_web
```

3. Open Chrome/Edge on Windows:

```text
http://localhost:8080
```

4. Open **Open Applications** in the dashboard. Native Windows visible applications should appear there while the main CPU/RAM/process monitor continues to represent Linux/WSL.

Windows normally forwards localhost to the WSL2 service.

## Security of the Windows bridge

The PowerShell command is hard-coded in the C backend and launched with `fork()` + `exec*()` argument arrays. Browser input is not appended to the PowerShell command, and the endpoint does not provide arbitrary Windows command execution. The bridge is read-only and enumerates visible process/window metadata.

## Remote SSH from WSL2

Install the OpenSSH client inside WSL:

```bash
sudo apt update
sudo apt install openssh-client -y
```

Generate/use the SSH key **inside the environment where `monitor_web` runs**. The backend launches that environment's `ssh` binary.

## Monitoring another Linux laptop

The remote Linux laptop still only needs OpenSSH server and an authorized account. Enter its reachable hostname/LAN IPv4 in the web dialog. If WSL networking cannot reach the remote host, first make normal `ssh user@REMOTE_IP` work in the WSL terminal.

Remote Open Applications detection remains Linux-side and uses authorized `/proc/<PID>/environ` + `/proc/<PID>/maps` evidence. Exact remote desktop window titles are not claimed because `/proc` does not expose compositor window titles.
