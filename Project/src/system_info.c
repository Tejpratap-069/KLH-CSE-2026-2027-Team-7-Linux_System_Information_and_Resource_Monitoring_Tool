#define _GNU_SOURCE
#include "monitor.h"
#include "utils.h"
#include "application_monitor.h"
#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <poll.h>
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/statvfs.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <unistd.h>



static int powershell_path(char *out, size_t cap) {
    const char *paths[] = {
        "/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe",
        "/mnt/c/Program Files/PowerShell/7/pwsh.exe",
        NULL
    };
    for (int i = 0; paths[i]; ++i) {
        if (access(paths[i], X_OK) == 0) {
            snprintf(out, cap, "%s", paths[i]);
            return 0;
        }
    }
    snprintf(out, cap, "%s", "powershell.exe");
    return 0;
}

static int read_windows_host_cpu_uptime(double *cpu_percent, double *uptime_seconds) {
    if (!cpu_percent || !uptime_seconds) return -1;
    static const char *script =
        "[Console]::OutputEncoding=[Text.UTF8Encoding]::new($false);"
        "$ErrorActionPreference='Stop';"
        "$cpu=(Get-Counter '\\Processor(_Total)\\% Processor Time').CounterSamples[0].CookedValue;"
        "$os=Get-CimInstance Win32_OperatingSystem;"
        "$up=((Get-Date)-$os.LastBootUpTime).TotalSeconds;"
        "('CPU|{0}' -f $cpu.ToString('F3',[Globalization.CultureInfo]::InvariantCulture));"
        "('UP|{0}' -f $up.ToString('F0',[Globalization.CultureInfo]::InvariantCulture));";

    int pfd[2];
    if (pipe(pfd) != 0) return -1;
    pid_t pid = fork();
    if (pid < 0) {
        close(pfd[0]); close(pfd[1]);
        return -1;
    }
    if (pid == 0) {
        dup2(pfd[1], STDOUT_FILENO);
        dup2(pfd[1], STDERR_FILENO);
        close(pfd[0]);
        close(pfd[1]);
        char exe[512];
        powershell_path(exe, sizeof(exe));
        char *const argv[] = {exe, "-NoLogo", "-NoProfile", "-NonInteractive", "-Command", (char*)script, NULL};
        execv(exe, argv);
        char *const fallback[] = {"powershell.exe", "-NoLogo", "-NoProfile", "-NonInteractive", "-Command", (char*)script, NULL};
        execvp("powershell.exe", fallback);
        _exit(127);
    }

    close(pfd[1]);
    int flags = fcntl(pfd[0], F_GETFL, 0);
    if (flags >= 0) fcntl(pfd[0], F_SETFL, flags | O_NONBLOCK);

    char buf[8192];
    size_t len = 0;
    int status = 0, done = 0, eof = 0, timed = 0;
    uint64_t start = now_ms();
    while (!done || !eof) {
        struct pollfd fd = { pfd[0], POLLIN | POLLHUP, 0 };
        poll(&fd, 1, 80);
        for (;;) {
            ssize_t n = read(pfd[0], buf + len, sizeof(buf) - len - 1);
            if (n > 0) {
                len += (size_t)n;
                if (len >= sizeof(buf) - 1) {
                    kill(pid, SIGKILL);
                    waitpid(pid, &status, 0);
                    close(pfd[0]);
                    return -1;
                }
                buf[len] = '\0';
                continue;
            }
            if (n == 0) eof = 1;
            break;
        }
        if (!done) {
            pid_t w = waitpid(pid, &status, WNOHANG);
            if (w == pid) done = 1;
        }
        if (now_ms() - start > 3000) {
            kill(pid, SIGKILL);
            waitpid(pid, &status, 0);
            timed = 1;
            done = 1;
        }
        if (done && eof) break;
    }
    close(pfd[0]);
    if (timed || !WIFEXITED(status) || WEXITSTATUS(status) != 0) return -1;

    int have_cpu = 0, have_uptime = 0;
    char *save = NULL;
    for (char *line = strtok_r(buf, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        trim_newline(line);
        if (strncmp(line, "CPU|", 4) == 0) {
            *cpu_percent = strtod(line + 4, NULL);
            have_cpu = 1;
        } else if (strncmp(line, "UP|", 3) == 0) {
            *uptime_seconds = strtod(line + 3, NULL);
            have_uptime = 1;
        }
    }
    return (have_cpu && have_uptime) ? 0 : -1;
}

static int read_cpu_ticks(uint64_t *total, uint64_t *idle) {
    char buf[512]; size_t n = 0;
    if (read_text_file("/proc/stat", buf, sizeof(buf), &n) != 0) return -1;
    unsigned long long user=0,nice=0,sys=0,idlev=0,iowait=0,irq=0,softirq=0,steal=0;
    int c = sscanf(buf, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
                   &user,&nice,&sys,&idlev,&iowait,&irq,&softirq,&steal);
    if (c < 4) return -1;
    *idle = idlev + iowait;
    *total = user + nice + sys + idlev + iowait + irq + softirq + steal;
    return 0;
}

static void read_cpu_model(char *out, size_t cap, int *cores) {
    char *buf = NULL; size_t len = 0;
    out[0] = '\0'; *cores = 0;
    if (read_text_file_alloc("/proc/cpuinfo", 1024*1024, &buf, &len) != 0) return;
    char *save = NULL;
    for (char *line = strtok_r(buf, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        if (strncmp(line, "processor", 9) == 0) (*cores)++;
        if (!out[0] && strncmp(line, "model name", 10) == 0) {
            char *colon = strchr(line, ':');
            if (colon) {
                colon++; while (*colon == ' ' || *colon == '\t') colon++;
                snprintf(out, cap, "%s", colon);
            }
        }
    }
    free(buf);
    if (*cores <= 0) {
        long c = sysconf(_SC_NPROCESSORS_ONLN);
        *cores = c > 0 ? (int)c : 1;
    }
    if (!out[0]) snprintf(out, cap, "Unknown CPU");
}

static void read_distro(char *out, size_t cap) {
    char *buf = NULL; size_t len = 0;
    out[0] = '\0';
    if (read_text_file_alloc("/etc/os-release", 65536, &buf, &len) == 0) {
        char *save = NULL;
        for (char *line = strtok_r(buf, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
            if (strncmp(line, "PRETTY_NAME=", 12) == 0) {
                char *v = line + 12;
                if (*v == '"') {
                    v++; char *e = strrchr(v, '"'); if (e) *e = '\0';
                }
                snprintf(out, cap, "%s", v);
                break;
            }
        }
        free(buf);
    }
    if (!out[0]) snprintf(out, cap, "Linux");
}

static int read_meminfo(SystemInfo *info) {
    char *buf = NULL; size_t len = 0;
    if (read_text_file_alloc("/proc/meminfo", 131072, &buf, &len) != 0) return -1;
    uint64_t total=0, avail=0, swapt=0, swapf=0;
    char *save = NULL;
    for (char *line = strtok_r(buf, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        unsigned long long v=0;
        if (sscanf(line, "MemTotal: %llu kB", &v)==1) total=v;
        else if (sscanf(line, "MemAvailable: %llu kB", &v)==1) avail=v;
        else if (sscanf(line, "SwapTotal: %llu kB", &v)==1) swapt=v;
        else if (sscanf(line, "SwapFree: %llu kB", &v)==1) swapf=v;
    }
    free(buf);
    info->mem_total_kb = total;
    info->mem_available_kb = avail;
    info->mem_used_kb = total >= avail ? total - avail : 0;
    info->mem_percent = total ? (100.0 * (double)info->mem_used_kb / (double)total) : 0.0;
    info->swap_total_kb = swapt;
    info->swap_free_kb = swapf;
    info->swap_used_kb = swapt >= swapf ? swapt - swapf : 0;
    info->swap_percent = swapt ? (100.0 * (double)info->swap_used_kb / (double)swapt) : 0.0;
    return 0;
}

static void read_network(SystemInfo *info, const SystemInfo *prev, double interval) {
    DIR *d = opendir("/sys/class/net");
    if (!d) return;
    struct ifaddrs *ifaddr = NULL;
    getifaddrs(&ifaddr);
    uint64_t rx=0, tx=0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL && info->interface_count < MAX_INTERFACES) {
        if (de->d_name[0] == '.') continue;
        NetInterfaceInfo *ni = &info->interfaces[info->interface_count];
        memset(ni, 0, sizeof(*ni));
        snprintf(ni->name, sizeof(ni->name), "%.*s", (int)sizeof(ni->name)-1, de->d_name);
        char path[512], buf[128]; size_t n=0;
        snprintf(path, sizeof(path), "/sys/class/net/%s/operstate", de->d_name);
        if (read_text_file(path, buf, sizeof(buf), &n)==0) { trim_newline(buf); snprintf(ni->operstate,sizeof(ni->operstate),"%.*s",(int)sizeof(ni->operstate)-1,buf); }
        else snprintf(ni->operstate,sizeof(ni->operstate),"unknown");
        snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/rx_bytes", de->d_name);
        if (read_text_file(path, buf, sizeof(buf), &n)==0) ni->rx_bytes = strtoull(buf,NULL,10);
        snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/tx_bytes", de->d_name);
        if (read_text_file(path, buf, sizeof(buf), &n)==0) ni->tx_bytes = strtoull(buf,NULL,10);
        for (struct ifaddrs *ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr || strcmp(ifa->ifa_name, de->d_name)!=0 || ifa->ifa_addr->sa_family != AF_INET) continue;
            char ip[INET_ADDRSTRLEN];
            struct sockaddr_in *sin=(struct sockaddr_in*)ifa->ifa_addr;
            if (inet_ntop(AF_INET,&sin->sin_addr,ip,sizeof(ip))) snprintf(ni->ipv4,sizeof(ni->ipv4),"%s",ip);
            break;
        }
        rx += ni->rx_bytes; tx += ni->tx_bytes;
        info->interface_count++;
    }
    if (ifaddr) freeifaddrs(ifaddr);
    closedir(d);
    info->net_rx_bytes = rx; info->net_tx_bytes = tx;
    if (prev && interval > 0.0) {
        info->net_rx_rate_bps = rx >= prev->net_rx_bytes ? (double)(rx-prev->net_rx_bytes)/interval : 0.0;
        info->net_tx_rate_bps = tx >= prev->net_tx_bytes ? (double)(tx-prev->net_tx_bytes)/interval : 0.0;
    }
}

int collect_system_info(SystemInfo *info, const SystemInfo *prev, double interval_seconds, char *err, size_t errcap) {
    if (!info) return -1;
    memset(info,0,sizeof(*info));
    struct utsname uts;
    if (uname(&uts)==0) snprintf(info->kernel,sizeof(info->kernel),"%s",uts.release);
    if (gethostname(info->hostname,sizeof(info->hostname)-1)!=0) snprintf(info->hostname,sizeof(info->hostname),"unknown");
    read_distro(info->distro,sizeof(info->distro));
    read_cpu_model(info->cpu_model,sizeof(info->cpu_model),&info->cpu_cores);
    if (read_cpu_ticks(&info->cpu_total_ticks,&info->cpu_idle_ticks)!=0) {
        if (err && errcap) snprintf(err,errcap,"Unable to read /proc/stat");
        return -1;
    }
    if (prev && prev->cpu_total_ticks && info->cpu_total_ticks > prev->cpu_total_ticks) {
        uint64_t dt = info->cpu_total_ticks-prev->cpu_total_ticks;
        uint64_t di = info->cpu_idle_ticks>=prev->cpu_idle_ticks ? info->cpu_idle_ticks-prev->cpu_idle_ticks : 0;
        info->cpu_percent = dt ? 100.0*(double)(dt-di)/(double)dt : 0.0;
    }
    char buf[256]; size_t n=0;
    if (read_text_file("/proc/loadavg",buf,sizeof(buf),&n)==0) sscanf(buf,"%lf %lf %lf",&info->load1,&info->load5,&info->load15);
    if (read_text_file("/proc/uptime",buf,sizeof(buf),&n)==0) sscanf(buf,"%lf",&info->uptime_seconds);
    if (is_wsl_environment()) {
        double host_cpu = 0.0, host_uptime = 0.0;
        if (read_windows_host_cpu_uptime(&host_cpu, &host_uptime) == 0) {
            info->cpu_percent = host_cpu;
            info->uptime_seconds = host_uptime;
        }
    }
    if (read_meminfo(info)!=0) { if(err&&errcap) snprintf(err,errcap,"Unable to read /proc/meminfo"); return -1; }
    struct statvfs vfs;
    if (statvfs("/",&vfs)==0) {
        uint64_t block=(uint64_t)vfs.f_frsize;
        info->disk_total_bytes=(uint64_t)vfs.f_blocks*block;
        info->disk_free_bytes=(uint64_t)vfs.f_bavail*block;
        info->disk_used_bytes=info->disk_total_bytes>=info->disk_free_bytes?info->disk_total_bytes-info->disk_free_bytes:0;
        info->disk_percent=info->disk_total_bytes?100.0*(double)info->disk_used_bytes/(double)info->disk_total_bytes:0.0;
    }
    read_network(info,prev,interval_seconds);
    return 0;
}
