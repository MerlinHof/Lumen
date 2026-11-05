#ifndef LUMEN_SERVICES_H
#define LUMEN_SERVICES_H
#include <stddef.h>
enum {
    SERVICE_WEATHER = 1,
    SERVICE_HA,
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
/* Bounded JSON tree. Object lookups never traverse into sibling objects. */
typedef struct {
    int start, end, next, count, type;
} JsonToken;
typedef struct {
    const char *text;
    JsonToken *tokens;
    int count, capacity;
} Json;
int json_parse(Json *j, const char *text, JsonToken *tokens, int cap);
int json_get(const Json *j, int object, const char *key);
int json_at(const Json *j, int array, int index);
void json_string(const Json *j, int token, char *out, int cap);
double json_number(const Json *j, int token);
#endif
