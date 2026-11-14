#ifndef LUMEN_SERVICES_H
#define LUMEN_SERVICES_H
#include <stddef.h>
enum {
    SERVICE_HA = 2,
    SERVICE_TOGGLE,
    SERVICE_TIME,
    SERVICE_WIKI_IMAGE,
    SERVICE_HA_HISTORY
};
int service_start(int kind, const char *arg, const char *token, const char *entity);
int service_poll(void); /* 0 pending/idle, kind success, -kind failure */
int service_busy(void);
int service_history_busy(void);
void service_cancel(void);
const char *service_error(void);
char *service_read(const char *name, size_t max);
int service_url_valid(const char *url);
int system_set_time(int year, int month, int day, int hour, int minute);
#include "../../tools/sdk/lumen_json.h"
#endif
