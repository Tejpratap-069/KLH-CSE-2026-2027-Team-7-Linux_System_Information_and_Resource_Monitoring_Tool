#ifndef MONITOR_H
#define MONITOR_H

#include <pthread.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <time.h>

#define MAX_PROCESSES 2048
#define MAX_LIFECYCLE_EVENTS 512
#define MAX_TRACKED_PROCESSES 2048
#define MAX_INTERFACES 32
#define MAX_MACHINE_NAME 64
#define MAX_HOST 256
#define MAX_USER 64
#define MAX_CMDLINE 512
#define MAX_PATH_TEXT 1024
#define MAX_IF_TEXT 512

#define SOURCE_LOCAL 0
#define SOURCE_REMOTE 1

typedef struct {
    char name[32];
    char operstate[16];
    char ipv4[64];
    uint64_t rx_bytes;
    uint64_t tx_bytes;
} NetInterfaceInfo;

typedef struct {
    char hostname[256];
    char distro[256];
    char kernel[128];
    char cpu_model[256];
    int cpu_cores;
    double cpu_percent;
    double load1;
    double load5;
    double load15;
    uint64_t mem_total_kb;
    uint64_t mem_available_kb;
    uint64_t mem_used_kb;
    double mem_percent;
    uint64_t swap_total_kb;
    uint64_t swap_free_kb;
    uint64_t swap_used_kb;
    double swap_percent;
    uint64_t disk_total_bytes;
    uint64_t disk_used_bytes;
    uint64_t disk_free_bytes;
    double disk_percent;
    double uptime_seconds;
    uint64_t cpu_total_ticks;
    uint64_t cpu_idle_ticks;
    int process_total;
    int process_running;
    int process_sleeping;
    long thread_total;
    uint64_t net_rx_bytes;
    uint64_t net_tx_bytes;
    double net_rx_rate_bps;
    double net_tx_rate_bps;
    int interface_count;
    NetInterfaceInfo interfaces[MAX_INTERFACES];
} SystemInfo;

typedef struct {
    pid_t pid;
    pid_t ppid;
    uid_t uid;
    char user[64];
    char name[256];
    char state;
    double cpu_percent;
    double ram_mb;
    double ram_percent;
    long threads;
    char cmdline[MAX_CMDLINE];
    char start_time[64];
    uint64_t cpu_ticks;
    uint64_t start_ticks;
    uint64_t rss_kb;
} ProcessInfo;

typedef struct {
    int source_type;
    char machine_id[MAX_MACHINE_NAME];
    char machine_name[MAX_MACHINE_NAME];
    char source_status[64];
    uint64_t timestamp_ms;
    uint64_t data_age_ms;
    SystemInfo system;
    size_t process_count;
    ProcessInfo processes[MAX_PROCESSES];
} MonitorSnapshot;

typedef struct {
    char event[16];
    uint64_t timestamp_ms;
    pid_t pid;
    pid_t ppid;
    uint64_t start_ticks;
    char name[256];
    double cpu_percent;
    double ram_mb;
    double peak_cpu_percent;
    double peak_ram_mb;
    uint64_t first_seen_ms;
    uint64_t last_seen_ms;
    uint64_t lifetime_ms;
    char final_state[16];
} LifecycleEvent;

typedef struct {
    int in_use;
    pid_t pid;
    pid_t ppid;
    uint64_t start_ticks;
    char name[256];
    uint64_t first_seen_ms;
    uint64_t last_seen_ms;
    uint64_t last_active_event_ms;
    double latest_cpu;
    double latest_ram;
    double peak_cpu;
    double peak_ram;
    int seen_this_round;
} TrackedProcess;

typedef int (*monitor_remote_collect_fn)(void *opaque, MonitorSnapshot *out, char *err, size_t errcap);

typedef struct {
    pthread_mutex_t lock;
    pthread_t worker;
    int worker_started;
    volatile sig_atomic_t stop_requested;
    unsigned interval_ms;
    MonitorSnapshot snapshot;
    MonitorSnapshot previous_snapshot;
    LifecycleEvent lifecycle[MAX_LIFECYCLE_EVENTS];
    size_t lifecycle_count;
    TrackedProcess tracked[MAX_TRACKED_PROCESSES];
    int selected_source;
    uint64_t selection_generation;
    char selected_machine_id[MAX_MACHINE_NAME];
    char selected_machine_name[MAX_MACHINE_NAME];
    void *remote_opaque;
    monitor_remote_collect_fn remote_collect;
    char last_error[256];
} MonitorContext;

int monitor_init(MonitorContext *ctx, unsigned interval_ms);
int monitor_start(MonitorContext *ctx);
void monitor_stop(MonitorContext *ctx);
void monitor_destroy(MonitorContext *ctx);
int monitor_set_local(MonitorContext *ctx);
int monitor_set_remote(MonitorContext *ctx, const char *machine_id, const char *machine_name,
                       monitor_remote_collect_fn cb, void *opaque);
int monitor_get_snapshot(MonitorContext *ctx, MonitorSnapshot *out);
size_t monitor_get_lifecycle(MonitorContext *ctx, LifecycleEvent *out, size_t max_count);
const ProcessInfo *monitor_find_process(const MonitorSnapshot *snap, pid_t pid);

int collect_local_snapshot(MonitorSnapshot *out, const MonitorSnapshot *prev, char *err, size_t errcap);
int collect_system_info(SystemInfo *info, const SystemInfo *prev, double interval_seconds, char *err, size_t errcap);
int collect_processes(ProcessInfo *out, size_t max_count, size_t *count,
                      uint64_t mem_total_kb, uint64_t cpu_total_ticks, uint64_t prev_cpu_total_ticks,
                      const ProcessInfo *prev, size_t prev_count, int cpu_cores,
                      int *running, int *sleeping, long *thread_total,
                      char *err, size_t errcap);
int inspect_process_json(pid_t pid, const MonitorSnapshot *snap, char **json_out);
int process_maps_json(pid_t pid, char **json_out);

#endif
