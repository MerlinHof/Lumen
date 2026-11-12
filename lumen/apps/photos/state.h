/* App-owned state; stable on-disk schemas are intentionally preserved. */
static char photo_names[512][256];
static int photo_count;
static char photo_loaded[256];
static int64_t photo_hour_drawn = -1;

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
static int photo_catalog_ready, photo_folder;
