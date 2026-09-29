# Likely Viva Questions and Concise Answers

1. **What is the main objective?** — Monitor real Linux resources while demonstrating OSSP concepts through C/POSIX programming.
2. **Why not use `top`?** — The C core reads kernel interfaces directly so the project demonstrates how monitoring actually works.
3. **What is `/proc/stat` used for?** — Aggregate CPU tick counters used for system and interval CPU calculations.
4. **What is `/proc/meminfo` used for?** — Physical RAM, available memory and swap statistics.
5. **How is used RAM calculated?** — `MemTotal - MemAvailable`.
6. **What is `/proc/<PID>/stat` used for?** — PID-related state, PPID, CPU ticks, start time and RSS pages.
7. **Why is parsing `/proc/<PID>/stat` tricky?** — The process name is in parentheses and may contain spaces, so field positions must be parsed after the closing parenthesis.
8. **What is PPID?** — The PID of a process's parent.
9. **What process states do you show?** — The raw Linux state character such as R, S, D, I, T or Z when visible.
10. **How do you calculate process CPU %?** — Delta process jiffies divided by delta aggregate CPU jiffies, multiplied by CPU count and 100.
11. **Why do you need two CPU samples?** — CPU utilization is a rate over an interval, not one absolute counter value.
12. **What happens on the first sample?** — CPU rate is 0/unavailable until a real second measurement provides a delta.
13. **How do you detect a new process?** — A `(PID,starttime)` key appears that was not in the tracked set.
14. **Why not identify a process by PID only?** — Linux can reuse a PID after a process exits.
15. **How do you detect exit?** — A previously tracked `(PID,starttime)` key disappears from the next process snapshot.
16. **Why is lifecycle history bounded?** — To avoid an ever-growing memory structure in a long-running monitor.
17. **What is VmSize?** — Total mapped virtual address-space size reported by Linux.
18. **What is VmRSS?** — Resident set size, the portion currently in physical RAM.
19. **What is `/proc/<PID>/maps`?** — The process's current virtual-memory mapping layout.
20. **What is a file descriptor?** — A per-process integer handle for an open file/socket/pipe.
21. **Where do you use `open/read/close`?** — Reading virtual Linux files such as `/proc/stat`, meminfo and process files.
22. **Where do you use `opendir/readdir`?** — Enumerating numeric process directories and file-descriptor/network directories.
23. **Where do you use `statvfs`?** — Reading filesystem capacity/available blocks for disk usage.
24. **Why use POSIX threads?** — Keep collection running while concurrent HTTP clients access prepared snapshots.
25. **What shared data needs a mutex?** — Current snapshot, process lifecycle history and tracking state.
26. **What is a race condition?** — Unsynchronized concurrent access whose result depends on execution timing.
27. **How does Ctrl+C work?** — `sigaction()` sets a stop flag; loops exit, the collector joins and the server closes normally.
28. **Explain the pipe demo.** — Parent creates a pipe and forks; child writes `/proc/uptime`; parent reads and waits with `waitpid()`.
29. **Why does the web button not accept a command?** — Arbitrary browser command execution would be unsafe and is unnecessary for the OSSP demonstration.
30. **Which socket calls are used?** — `socket`, `setsockopt`, `bind`, `listen` and `accept`.
31. **Why bind to localhost by default?** — It limits access to the current machine unless LAN exposure is explicitly requested.
32. **How does remote monitoring work without an agent?** — The main machine runs OpenSSH and sends a fixed read-only script over an authenticated session.
33. **Why key-based SSH?** — It supports non-interactive authorized access without transmitting/storing a password in the browser.
34. **How do you prevent SSH command injection?** — Strict validation plus `execvp()` argv; browser text is not concatenated into a local shell command.
35. **Does the remote laptop need the project ZIP?** — No; it only needs Linux, SSH server and an authorized account.
36. **What if SSH fails?** — The API reports authentication/unreachable/timeout/client-missing errors and keeps stale/local data separated.
37. **How is network rate calculated?** — Delta RX/TX bytes divided by elapsed seconds between real samples.
38. **What does `/sys/class/net` provide?** — Per-interface kernel/device attributes such as state and RX/TX counters.
39. **How is WSL2 different from native Ubuntu?** — `/proc` still represents the WSL Linux environment; native Windows apps are not falsely treated as Linux processes.
40. **How does Open Applications show Windows apps from WSL?** — The C backend invokes a fixed Windows PowerShell `Get-Process` query through WSL interop and returns only processes with visible top-level windows.
41. **Can the project see every Chrome tab?** — No. It can show the browser's visible main-window title, which may identify the current page/web app, but it does not enumerate every tab or URL.
42. **How are native Linux GUI apps detected without `wmctrl`?** — Real `/proc/<PID>/environ` must contain DISPLAY/WAYLAND_DISPLAY and `/proc/<PID>/maps` must show GUI client libraries such as X11/XCB, Wayland, GTK/GDK or Qt.
43. **Why is the PowerShell bridge safe from browser command injection?** — The command is a fixed C string passed via `exec*()` arguments; no browser field is concatenated into it.
44. **What proves the frontend values are real?** — Frontend JavaScript only renders backend API data; no random telemetry generation exists.

41. **How can your remote form use only an IP address?** — The browser sends only a strictly validated IPv4 address. OpenSSH resolves the authorized account/key from its normal configuration or backend-only defaults; the form itself never handles credentials.
42. **Does IP-only mean authentication is bypassed?** — No. SSH authorization is still mandatory. The UI is simpler, but the OS security boundary is unchanged.
43. **What happens after Quick Connect succeeds?** — The endpoint is saved if new, selected as the active machine, and all dashboard pages read only that remote machine's prepared snapshot.
