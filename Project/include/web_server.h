#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <stddef.h>
#include <signal.h>
#include "api.h"

typedef struct {
    int listen_fd;
    int port;
    int bind_lan;
    volatile sig_atomic_t *stop_flag;
    ApiContext *api;
    char frontend_dir[512];
    volatile int active_clients;
} WebServer;

int web_server_run(WebServer *server);

#endif
