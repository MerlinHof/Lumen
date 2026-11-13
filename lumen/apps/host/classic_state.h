/* App-owned module; assembled by apps/host/compose.h. */
/* Offline experimental apps. All timers stop off-screen and during sleep. */
#include <stdint.h>
#include <sys/stat.h>
#include "../../../tools/sdk/lumen_reading_activity.h"
static ReadingActivityV1 app_data;
static int apps_dirty;
static int64_t app_last_time;
static int64_t app_millis(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (int64_t)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}
static int app_date_key(time_t now) {
    struct tm t;
    localtime_r(&now, &t);
    return (t.tm_year + 1900) * 10000 + (t.tm_mon + 1) * 100 + t.tm_mday;
}
static ReadingDay *app_day(int key, int create) {
    int oldest = 0;
    for (int i = 0; i < 31; i++) {
        if (app_data.days[i].date == key)
            return &app_data.days[i];
        if (app_data.days[i].date < app_data.days[oldest].date)
            oldest = i;
    }
    if (!create)
        return NULL;
    memset(&app_data.days[oldest], 0, sizeof(ReadingDay));
    app_data.days[oldest].date = key;
    return &app_data.days[oldest];
}
