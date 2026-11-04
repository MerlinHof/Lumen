/* App-owned state; stable on-disk schemas are intentionally preserved. */
static char photo_names[512][256];
static int photo_count, photo_page, photo_selected, photo_cache_page = -1;
static char photo_loaded[256];
static int64_t photo_hour_drawn = -1;
static unsigned char photo_thumbs[6][168 * 224];
