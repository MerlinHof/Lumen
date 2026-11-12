/* App-owned module; assembled by apps/host/compose.h. */
#include "../news/archive_api.h"
#include "../games/puzzles.h"
static unsigned new_dirty;
static unsigned news_topics = 63;
static int news_enabled, news_page, news_done, news_settings_loaded, news_source_page,
    news_puzzle_revealed;
static time_t news_retry;
