#ifndef REMOTE_SSH_H
#define REMOTE_SSH_H

#include <stddef.h>
#include <sys/types.h>
#include "machine_store.h"
#include "monitor.h"

typedef struct {
    pthread_mutex_t lock;
    MachineConfig machine;
    char identity_file[512];
    MonitorSnapshot previous;
    int have_previous;
} RemoteCollector;

int remote_test_connection(const MachineConfig *m, const char *identity_file, char **json_out);
int remote_collect_snapshot(RemoteCollector *rc, MonitorSnapshot *out, char *err, size_t errcap);
int remote_inspect_process_json(const MachineConfig *m, const char *identity_file, pid_t pid, const MonitorSnapshot *snap, char **json_out);
int remote_process_maps_json(const MachineConfig *m, const char *identity_file, pid_t pid, char **json_out);
int remote_open_applications_json(const MachineConfig *m, const char *identity_file, const MonitorSnapshot *snap, char **json_out);
int ssh_available(void);

#endif
