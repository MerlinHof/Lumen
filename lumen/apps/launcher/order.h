/* App-owned module; assembled by apps/host/compose.h. */
/* 1.1 app polish: shared chrome, permanent reading caches, deterministic flows. */
#include "../news/api.h"
static int power_menu, power_hold, power_request;
static int64_t power_menu_wake_until;
static int polish_order_done;
static int tree_id[1536], tree_type[1536], tree_count, tree_page;
static void polish_init(void) {}
static void polish_order(void) {
    if (polish_order_done)
        return;
    polish_order_done = 1;
    char p[2048];
    local_path(p, sizeof(p), "launcher-order16.txt");
    if (!access(p, F_OK))
        return;
    const int first[] = {APP_CALCULATOR, APP_WIKIPEDIA,  APP_QUOTES,         APP_NEWS,
                         APP_CALENDAR,   APP_TIMER,      APP_NOTES,          APP_WEATHER,
                         APP_INSIGHTS,   APP_PHOTOS,     APP_HOME_ASSISTANT, APP_TASKS,
                         APP_LANGUAGE,   APP_FLASHCARDS, APP_PLANETARIUM,    APP_JOURNAL,
                         APP_STUDY,      APP_RECIPES,    APP_PERIODIC,       APP_FILES,
                         APP_LLM,        APP_FOCUS};
    int order[APP_COUNT], seen[APP_COUNT] = {0}, n = 0;
    for (unsigned i = 0; i < sizeof(first) / sizeof(*first); i++) {
        order[n++] = first[i];
        seen[first[i]] = 1;
    }
    for (int i = 0; i < APP_COUNT; i++) {
        int id = launcher_order[i];
        if (!seen[id] && (id < APP_MINES || id == APP_STATUS_BAR ||
                          id == APP_LOCK_SCREEN || id == APP_KEYBOARD)) {
            order[n++] = id;
            seen[id] = 1;
        }
    }
    for (int i = 0; i < APP_COUNT; i++)
        if (!seen[launcher_order[i]])
            order[n++] = launcher_order[i];
    memcpy(launcher_order, order, sizeof(order));
    v1_dirty = 1;
    v1_save();
    atomic_write(p, "1", 1);
}
