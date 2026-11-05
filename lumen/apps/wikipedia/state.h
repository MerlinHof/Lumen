/* App-owned state; stable on-disk schemas are intentionally preserved. */
static char wiki_query[256],wiki_image_requested[2048];
static time_t wiki_image_retry_at;static int wiki_image_attempts,wiki_page;
static int wiki_image_available(void);
