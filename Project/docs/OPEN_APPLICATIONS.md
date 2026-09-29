# Open Applications and Browser/Web-App Visibility

## Purpose

The normal Process Monitor is intentionally low-level: it shows Linux PIDs and kernel-exposed process metrics. The **Open Applications** section adds a user-facing view that answers a different question: *which desktop applications are open right now?*

## WSL2 + Windows host

When the backend detects WSL2, it does not pretend Windows applications exist in Linux `/proc`. Instead, it launches Windows PowerShell through normal WSL interop using a **fixed command embedded in the C backend**. The query uses `Get-Process` and selects only processes with a non-zero `MainWindowHandle` and a non-empty `MainWindowTitle`.

The backend measures CPU using two process CPU-time samples separated by 200 ms and normalizes by the Windows logical processor count. RAM comes from the process working set. The browser never supplies a command string.

Example of real data returned on a Windows host:

```text
Google Chrome | Windows PID | CPU % | RAM | ChatGPT - Google Chrome
Visual Studio Code | Windows PID | CPU % | RAM | project-folder - Visual Studio Code
```

The browser title may identify the currently visible page or web app. It is not a URL and it is not a list of all browser tabs.

## Native Linux

To remain dependency-light, the project does not require `wmctrl`, `xdotool`, Xlib or compositor-specific SDKs. A process is considered a GUI-session application only when:

1. its real `/proc/<PID>/environ` contains `DISPLAY` or `WAYLAND_DISPLAY`; and
2. its real `/proc/<PID>/maps` contains GUI client libraries such as X11/XCB, Wayland, GTK/GDK or Qt GUI libraries.

This avoids treating every inherited desktop-session process as an application. Processes are grouped into friendly application rows, and their real process CPU/RAM are aggregated. Exact window titles are not claimed in this mode.

## Remote Linux over SSH

The remote collector uses a fixed read-only script over already-authorized SSH. It checks the same `/proc/<PID>/environ` and `/proc/<PID>/maps` evidence, then correlates returned PIDs with the existing remote snapshot. No project agent is installed remotely.

## Security boundary

- No password capture.
- No browser-supplied shell/PowerShell command.
- No hidden persistence.
- No browser history collection.
- No arbitrary remote command console.
- No fabricated application/window rows.
