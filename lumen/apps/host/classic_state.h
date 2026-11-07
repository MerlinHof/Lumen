/* App-owned module; assembled by apps/host/compose.h. */
/* Offline experimental apps. All timers stop off-screen and during sleep. */
#include <stdint.h>
#include <sys/stat.h>
typedef struct {
    int date;
    unsigned seconds, pages;
} ReadingDay;
typedef struct {
    unsigned magic, version, seconds, pages, best_snake, best_blocks, mines_wins, word_wins;
    ReadingDay days[31];
} AppData;
static AppData app_data;
static Mines mine_game;
static Hangman word_game;
static Blocks block_game;
static Snake snake_game;
static int apps_dirty, app_ready[7], mine_flags, mine_release, mine_recorded, word_recorded;
static int calendar_view16=2;
static int calendar_year, calendar_month, calendar_day;
static char calendar_note[512];
static unsigned calendar_note_days;
static int64_t app_last_time, app_next_tick;
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
