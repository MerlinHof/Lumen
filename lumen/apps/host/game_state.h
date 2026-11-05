/* App-owned module; assembled by apps/host/compose.h. */
#include "../news/archive_api.h"
#include "../games/puzzles.h"
#include "../calculator/plots.h"
#include "../2048/state.h"
#include "../solitaire/state.h"
#include "../sudoku/state.h"
static unsigned new_dirty;
static int new_ready[3];
typedef struct {
    int span, level, sessions, streak[4], failed[4], scores[30], days[30], times[30];
} FocusStats;
static FocusStats focus_stats;
static uint32_t pro_rng;
static unsigned news_topics = 63;
static int news_enabled, news_page, news_done, news_settings_loaded, news_source_page,
    news_puzzle_revealed;
static time_t news_retry;
