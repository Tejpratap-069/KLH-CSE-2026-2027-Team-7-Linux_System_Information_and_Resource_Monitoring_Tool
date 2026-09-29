# Faculty Explanation — Simple Speaking Version

## What is the project?

Our project is a real-time Linux System Information and Resource Monitoring Tool. It shows CPU, RAM, swap, disk, uptime, load, network and live process details. The main purpose is not only monitoring; it demonstrates how a user-space C program gets services and information from the Linux operating system.

## Why did we use C?

C is suitable for systems programming because it gives direct access to POSIX/Linux APIs, file descriptors, sockets, processes, threads, signals and memory. Our course is Operating Systems and Systems Programming, so the core remains in C rather than replacing it with a web framework.

## What is `/proc`?

`/proc` is a virtual filesystem created by the Linux kernel. Files such as `/proc/stat`, `/proc/meminfo` and `/proc/<PID>/stat` expose live kernel/process information. They are not ordinary disk files even though user-space programs read them using file operations.

## What is `/sys`?

`/sys` is another kernel-created filesystem that exposes devices and kernel objects. We use `/sys/class/net` for network interface state and RX/TX byte counters.

## How is CPU calculated?

For system CPU, we read the aggregate CPU counters in `/proc/stat` twice. CPU % is calculated from the change in total ticks minus the change in idle ticks. For one process, we read `utime + stime` from `/proc/<PID>/stat` twice and divide that delta by the system total-jiffies delta, then multiply by the number of CPU cores and 100.

## How is RAM calculated?

`/proc/meminfo` gives `MemTotal` and `MemAvailable`. We calculate used RAM as `MemTotal - MemAvailable`, then convert it to a percentage. Swap uses `SwapTotal - SwapFree`.

## How do you identify processes?

We scan numeric directories inside `/proc`. Each `/proc/<PID>` directory represents one visible process. We read the process stat/status files for its details.

## What is PID and PPID?

PID is Process ID. PPID is Parent Process ID. Together they help show parent-child relationships, but PID alone is not enough to permanently identify a process because Linux can reuse PID numbers.

## How do you know a process opened?

We compare the current process snapshot with the previous tracked set. If `(PID, starttime)` did not exist previously, it is a new process instance, so we record `STARTED`.

## How do you know it closed?

If a previously tracked `(PID, starttime)` pair is absent from the new snapshot, we record `EXITED` and its lifetime/peak values.

## How do you handle PID reuse?

We use the PID together with the process start-time ticks from `/proc/<PID>/stat`. A reused PID with a different start time is treated as a new process, not as the old one continuing.

## What are file descriptors?

A file descriptor is a small integer used by a process to refer to an open file/socket/pipe. Our code obtains FDs from `open()`, `socket()` and `pipe()`, uses them for `read()`/`write()`, and closes them when finished.

## Where are system calls used?

Examples include file I/O (`open`, `read`, `close`), process/IPC (`fork`, `pipe`, `waitpid`), sockets (`socket`, `bind`, `listen`, `accept`), and signals (`sigaction`). POSIX library calls expose these kernel services to our user-space program.

## Where is process management used?

The monitor reads process IDs/states continuously. The lifecycle engine observes creation and termination. The IPC demo creates a child with `fork()` and synchronizes with `waitpid()`. The SSH worker also uses controlled child-process execution.

## Where is virtual memory shown?

The process inspector reads VmSize, VmRSS, RssAnon, RssFile, VmData, VmStk, VmExe, VmLib and VmSwap, and can show `/proc/<PID>/maps`.

## What is virtual memory vs resident memory?

Virtual memory is the address space mapped for a process. Resident memory is the portion currently present in physical RAM. A process can have a large virtual size but a much smaller resident set.

## Why pthreads?

Monitoring must continue while web requests are being served. We use a collector thread to refresh the snapshot and separate HTTP worker threads so one browser request does not stop monitoring.

## Why mutexes?

The collector writes shared snapshot/history while HTTP threads read it. Without a mutex, one thread could read a structure halfway through an update, which is a race condition.

## What is a race condition?

A race condition happens when multiple threads access shared data and the result depends on timing. We avoid it by locking the monitor mutex before reading or replacing shared snapshot/lifecycle state.

## How does the IPC demo work?

The parent creates a pipe, then forks. The child reads `/proc/uptime` and writes a message into the pipe. The parent reads the message and calls `waitpid()` to collect the child's exit status. This visibly demonstrates process creation, IPC and synchronization.

## How does another laptop get monitored?

The main backend runs the local `ssh` client using `fork/exec` and pipes. It authenticates normally to an authorized Linux laptop and sends a fixed read-only collection script. That script reads the remote laptop's `/proc` and `/sys` and sends measurements back. We do not copy the whole project to the remote laptop.

### Why does the website ask only for the IP address now?

The simplified **IP-only Quick Connect** UI sends only a validated IPv4 address to the C backend. SSH account/key selection is handled by OpenSSH configuration or backend environment defaults, so the browser does not need username, password, port, or key fields. This is only a UI simplification; the remote laptop still has to authorize SSH once.

## Why must the remote laptop authorize SSH?

Because monitoring another machine is only legitimate after normal authentication. Our program does not bypass login or capture passwords; it uses the user's already-authorized SSH key.

## Is the data fake?

No. The frontend has no `Math.random()` telemetry. It only renders JSON returned from the C backend. If a value is not available, the UI shows unavailable/no data rather than inventing a number.

## Why does the normal WSL process table show Linux processes rather than every Windows process?

Because `/proc` belongs to the Linux environment in WSL2, so the main OSSP process table correctly shows Linux/WSL processes. We do not pretend Windows processes are inside Linux `/proc`.

## Then how does the Open Applications section show Chrome or VS Code on Windows?

That is a separate WSL integration feature. The C backend detects WSL and launches a fixed Windows PowerShell `Get-Process` query through WSL interop. It selects Windows processes that own visible top-level windows and returns their Windows PID, CPU, RAM and main-window title. The browser does not send a command to PowerShell. This preserves the Linux systems-programming core while still making the dashboard useful in a Windows + WSL demo.

## Can it show which website is open?

It can show the visible browser **window title**, for example `ChatGPT - Google Chrome`, so the UI can display `ChatGPT` as a web-app/page-title hint. It does not claim to read every browser tab, the exact URL, browsing history, or hidden/incognito pages.
