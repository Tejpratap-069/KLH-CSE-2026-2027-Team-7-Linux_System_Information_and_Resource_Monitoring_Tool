#ifndef MACHINE_STORE_H
#define MACHINE_STORE_H

#include <pthread.h>
#include <stddef.h>

#define MAX_MACHINES 16

typedef struct {
    char id[64];
    char display_name[64];
    char host[256];
    char username[64];
    int port;
    int is_local;
} MachineConfig;

typedef struct {
    pthread_mutex_t lock;
    char config_path[512];
    MachineConfig machines[MAX_MACHINES];
    size_t count;
    char selected_id[64];
} MachineStore;

int machine_store_init(MachineStore *store, const char *path);
void machine_store_destroy(MachineStore *store);
int machine_store_load(MachineStore *store);
int machine_store_save(MachineStore *store);
size_t machine_store_list(MachineStore *store, MachineConfig *out, size_t max_count);
int machine_store_get(MachineStore *store, const char *id, MachineConfig *out);
int machine_store_add(MachineStore *store, const MachineConfig *machine, char *err, size_t errcap);
int machine_store_delete(MachineStore *store, const char *id, char *err, size_t errcap);
int machine_store_select(MachineStore *store, const char *id, char *err, size_t errcap);
int validate_machine_config(const MachineConfig *m, char *err, size_t errcap);
void machine_make_id(const char *display_name, const char *host, char *out, size_t outcap);

#endif
