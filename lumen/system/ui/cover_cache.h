/* Bounded in-memory display tiles. Never writes another disk cache or reduces
 * source resolution. Exact source metadata and dither phase invalidate tiles. */
#define COVER_TILE_LIMIT 8
#define COVER_TILE_PIXELS 80000
static struct {
    struct stat source;
    int w, h, crop, phase;
    unsigned char *pixels;
} cover_tiles[COVER_TILE_LIMIT];
static unsigned cover_tile_next;
static long cover_stamp_ns(const struct stat *s) {
#ifdef __APPLE__
    return s->st_mtimespec.tv_nsec;
#else
    return s->st_mtim.tv_nsec;
#endif
}
static int cover_tile_safe(int x, int y, int w, int h) {
    return x >= 0 && y >= 0 && w > 0 && h > 0 && x + w <= WIDTH && y + h <= HEIGHT &&
           w * h <= COVER_TILE_PIXELS;
}
static int cover_tile_get(const struct stat *source, int x, int y, int w, int h) {
    int phase = (x & 3) + 4 * (y & 3);
    y += ui_y_offset;
    if (!cover_tile_safe(x, y, w, h))
        return 0;
    for (int i = 0; i < COVER_TILE_LIMIT; i++) {
        if (!cover_tiles[i].pixels || cover_tiles[i].w != w || cover_tiles[i].h != h ||
            cover_tiles[i].crop != options[OPT_COVER_CROP] || cover_tiles[i].phase != phase)
            continue;
        const struct stat *s = &cover_tiles[i].source;
        if (s->st_dev != source->st_dev || s->st_ino != source->st_ino ||
            s->st_size != source->st_size || s->st_mtime != source->st_mtime ||
            cover_stamp_ns(s) != cover_stamp_ns(source))
            continue;
        for (int j = 0; j < h; j++)
            memcpy(ui_pixels + (y + j) * WIDTH + x, cover_tiles[i].pixels + j * w, w);
        return 1;
    }
    return 0;
}
static void cover_tile_put(const struct stat *source, int x, int y, int w, int h) {
    int phase = (x & 3) + 4 * (y & 3);
    y += ui_y_offset;
    if (!cover_tile_safe(x, y, w, h))
        return;
    unsigned i = cover_tile_next++ % COVER_TILE_LIMIT;
    free(cover_tiles[i].pixels);
    cover_tiles[i].pixels = malloc((size_t)w * h);
    if (!cover_tiles[i].pixels)
        return;
    cover_tiles[i].source = *source;
    cover_tiles[i].w = w;
    cover_tiles[i].h = h;
    cover_tiles[i].crop = options[OPT_COVER_CROP];
    cover_tiles[i].phase = phase;
    for (int j = 0; j < h; j++)
        memcpy(cover_tiles[i].pixels + j * w, ui_pixels + (y + j) * WIDTH + x, w);
}
static void cover_tiles_clear(void) {
    for (int i = 0; i < COVER_TILE_LIMIT; i++) {
        free(cover_tiles[i].pixels);
        cover_tiles[i].pixels = NULL;
    }
    cover_tile_next = 0;
}
