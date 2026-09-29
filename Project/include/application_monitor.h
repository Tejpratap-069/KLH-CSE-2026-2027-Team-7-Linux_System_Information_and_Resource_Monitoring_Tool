#ifndef APPLICATION_MONITOR_H
#define APPLICATION_MONITOR_H

#include <stddef.h>
#include "monitor.h"

int is_wsl_environment(void);
int local_open_applications_json(const MonitorSnapshot *snap, char **json_out, char *err, size_t errcap);
const char *application_kind_for_name(const char *name);
void application_friendly_name(const char *name, char *out, size_t cap);
int application_name_is_noise(const char *name);

#endif
