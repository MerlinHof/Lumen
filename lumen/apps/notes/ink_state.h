/* App-owned module; assembled by apps/host/compose.h. */
/* Shared reader controls, catalog preferences and page-bound ink notes. */
static int64_t ink_refresh_at, ink_save_at;
static int u15_ready, note_ink_mode, ink_eraser, ink_width = 3, ink_note = -1, ink_page = -1,
                                                 ink_last_x, ink_last_y, ink_drag, ink_dirty;
static unsigned char ink_pixels[600 * 688], ink_undo[600 * 688];
static struct {
    char name[80], url[2048];
    int enabled;
} source15[10];
static int source15_count, source15_page, sources15_dirty;

static int note_offsets[8193], note_numbers[8193], note_rows;
static void ink_save(void) {
    if (ink_note < 0 || !ink_dirty)
        return;
    char p[2048];
    snprintf(p, sizeof(p), "%s/notes/%d-page-%d.ink", data_root, ink_note, ink_page);
    int empty = 1;
    for (size_t i = 0; i < sizeof(ink_pixels); i++)
        if (ink_pixels[i] != 255) {
            empty = 0;
            break;
        }
    if (empty) {
        unlink(p);
        ink_dirty = 0;
        return;
    }
    if (atomic_write(p, ink_pixels, sizeof(ink_pixels)))
        ink_dirty = 0;
}
static void u15_flush(void) {
    ink_save();
}
static void ink_load(void) {
    if (ink_note == note_selected && ink_page == extra_page)
        return;
    ink_save();
    ink_note = note_selected;
    ink_page = extra_page;
    memset(ink_pixels, 255, sizeof(ink_pixels));
    char p[2048];
    snprintf(p, sizeof(p), "%s/notes/%d-page-%d.ink", data_root, ink_note, ink_page);
    FILE *f = fopen(p, "rb");
    if (f) {
        if (fread(ink_pixels, 1, sizeof(ink_pixels), f) != sizeof(ink_pixels))
            memset(ink_pixels, 255, sizeof(ink_pixels));
        fclose(f);
    }
    ink_dirty = 0;
    memcpy(ink_undo, ink_pixels, sizeof(ink_pixels));
}
