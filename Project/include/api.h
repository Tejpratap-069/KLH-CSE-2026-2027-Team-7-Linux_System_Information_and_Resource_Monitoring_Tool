#ifndef API_H
#define API_H

#include <stddef.h>
#include "machine_store.h"
#include "monitor.h"
#include "remote_ssh.h"

typedef struct {
    MonitorContext *monitor;
    MachineStore *machines;
    RemoteCollector remote_collector;
    int remote_active;
    char identity_file[512];
} ApiContext;

typedef struct {
    int status;
    char content_type[64];
    char *body;
} ApiResponse;

void api_context_init(ApiContext *ctx, MonitorContext *monitor, MachineStore *machines);
void api_context_destroy(ApiContext *ctx);
void api_response_free(ApiResponse *r);
int api_handle(ApiContext *ctx, const char *method, const char *path, const char *body, ApiResponse *out);

#endif
