# OSSP Course Outcome Mapping — 25CS2104E

This mapping follows the course handout framing for CO1–CO6 and the approved project form.

## CO1 — OS as a Service Layer

**Handout themes:** operating system as a layered service abstraction, user space vs kernel space, system calls/kernel services, shell/user-space programs, Linux architecture and systems-programming fundamentals.

**In this project:** the monitor is a user-space C application. It requests information exposed by the Linux kernel through `/proc`, `/sys`, sockets, file descriptors and POSIX APIs. The browser is only a presentation layer; the systems interaction remains in C.

## CO2 — Processes and Process Control

**Handout themes:** process abstraction, process lifecycle/state transitions, creation, execution, synchronization, termination and common process-management pitfalls.

**In this project:** PID, PPID, states, process CPU/RAM, start time and threads are read from live process entries. The lifecycle engine compares snapshots, and the IPC demo uses `fork()` plus `waitpid()`. PID reuse is handled by combining PID with process start ticks.

## CO3 — IPC and Signals

**Handout themes:** anonymous pipes, signals/asynchronous notifications, signal handlers and POSIX signal programming.

**In this project:** the explicit demo uses `pipe()`, `fork()`, `read()`, `write()` and `waitpid()`. `sigaction()` is used for clean Ctrl+C/SIGTERM shutdown. SSH collection also uses parent/child pipes for controlled data transfer to/from the `ssh` process.

## CO4 — Memory Management

**Handout themes:** virtual memory, address translation concepts, demand paging, Linux process address-space layout, dynamic allocation, memory errors and analysis.

**In this project:** system RAM/available/swap are read from `/proc/meminfo`. Per-process resident memory comes from `/proc/<PID>/stat`/status. The inspector exposes VmSize, VmRSS, RssAnon, RssFile, VmData, VmStk, VmExe, VmLib, VmSwap and optionally `/proc/<PID>/maps`.

**Virtual vs resident:** virtual memory (for example VmSize) is the process's mapped virtual address space. Resident memory (VmRSS/RSS) is the part currently backed by physical RAM. They are not the same quantity.

## CO5 — File Systems and File I/O in Linux

**Handout themes:** Unix file abstraction, directory entries, VFS, file descriptors/open-file management and Linux file I/O system calls.

**In this project:** `open()`, `read()`, `close()`, `opendir()`, `readdir()`, `readlink()` and `statvfs()` are used to consume Linux virtual/system files and enumerate process/file-descriptor directories. The monitor directly demonstrates the “everything as a file-like interface” design of Linux.

## CO6 — Concurrency and Synchronization

**Handout themes:** threads vs processes, POSIX threads, race conditions/shared data, mutual exclusion and coordination.

**In this project:** `pthread_create()` runs the monitoring worker and HTTP client workers. The main `MonitorContext` snapshot, lifecycle history and tracking tables are shared state. `pthread_mutex_lock()`/`pthread_mutex_unlock()` protect that state so an HTTP request cannot read it while the collector is halfway through an update.
