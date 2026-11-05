/* App-owned state; stable on-disk schemas are intentionally preserved. */
static char photo_names[512][256];
static int photo_count, photo_page, photo_selected, photo_cache_page = -1;
static char photo_loaded[256];
static int64_t photo_hour_drawn = -1;
static unsigned char photo_thumbs[6][168 * 224];

enum { PHOTO_FOLDERS = 16, PHOTO_PER_PAGE = 16 };
typedef struct {
    uint64_t id;
    unsigned folders;
} PhotoMembership;
static struct {
    char folders[PHOTO_FOLDERS][64];
    PhotoMembership items[512];
    int seconds, fill, landscape;
} photo_catalog = {.seconds = 10};
static int photo_catalog_ready, photo_folder, photo_settings, photo_slideshow;
static int64_t photo_next_frame;
static uint64_t photo_assignment;
static int photo_menu_ids[PHOTO_FOLDERS + 1], photo_menu_count;
