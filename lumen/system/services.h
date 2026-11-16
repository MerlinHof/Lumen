#ifndef LUMEN_SERVICES_H
#define LUMEN_SERVICES_H
#include <stddef.h>
enum {
    SERVICE_TIME,
    SERVICE_WIKI_IMAGE
};
int service_start(int kind,const char *arg);
int service_poll(void); /* 0 pending/idle, kind success, -kind failure */
int service_busy(void);
void service_cancel(void);
const char *service_error(void);
char *service_read(const char *name, size_t max);
int system_set_time(int year, int month, int day, int hour, int minute);
#include "../../tools/sdk/lumen_json.h"
#endif
