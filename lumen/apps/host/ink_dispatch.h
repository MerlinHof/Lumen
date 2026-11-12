/* App-owned module; assembled by apps/host/compose.h. */
static int u15_commit(int target, const char *text) {
    if (u16_commit(target, text))
        return 1;
    return 0;
}
static int u15_input(const Input *in) {
    if (editor_ui.keyboard || dropdown)
        return -1;
    if (sleeping)
        return -1;
    if (ui_route() != 30 && ink_note >= 0) {
        ink_save();
        note_ink_mode = 0;
    }
    int x = in->x, y = in->y;
    if (in->kind == INPUT_SLEEP) {
        ink_save();
        ink_drag = 0;
    }
    if (ui_route() == 30 && !editor_ui.keyboard) {
        if (note_ink_mode) {
            if (in->kind==INPUT_HOLD)return 2;
            if (in->kind == INPUT_DOWN && y >= 112 && y < 742) {
                ink_load();
                memcpy(ink_undo, ink_pixels, sizeof(ink_pixels));
                ink_last_x = x;
                ink_last_y = y;
                ink_drag = 1;
                ink_stamp(x, y);
                return 1;
            }
            if (ink_drag &&
                (in->kind == INPUT_MOVE || in->kind == INPUT_TAP || in->kind == INPUT_SWIPE)) {
                if (y >= 112 && y < 742)
                    ink_line(x, y);
                if (in->kind == INPUT_MOVE) {
                    int64_t now = extra_millis();
                    if (now < ink_refresh_at)
                        return 2;
                    ink_refresh_at = now + 32;
                }
                if (in->kind != INPUT_MOVE) {
                    ink_drag = 0;
                    ink_save_at = extra_millis() + 1500;
                }
                return 1;
            }
        }
    }
    if (ui_route() == 36 && (discover_busy() || wiki_reader_pending) && in->kind == INPUT_TAP &&
        y >= 120)
        return 1;
    if (in->kind != INPUT_TAP)
        return -1;
    return -1;
}


static void u15_export_photo(void) {
    ink_save();
    memset(ui_pixels, 255, sizeof(ui_pixels));
    u15_note_editor_draw();
    for (int i = 0; i < 600 * 688; i++)
        if (ink_pixels[i] < ui_pixels[i + 54 * 600])
            ui_pixels[i + 54 * 600] = ink_pixels[i];
    char p[2048], dir[2048];
    local_path(dir, sizeof(dir), "photos");
    mkdir(dir, 0700);
    snprintf(p, sizeof(p), "%s/note-%d-%d-%lld.pgm", dir, note_selected, extra_page,
             (long long)time(NULL));
    unsigned char *buf = malloc(480064);
    if (buf) {
        int n = snprintf((char *)buf, 64, "P5\n600 800\n255\n");
        memcpy(buf + n, ui_pixels, 480000);
        if (atomic_write(p, buf, n + 480000))
            ui_message("Foto gespeichert.");
        free(buf);
    }
    v1_photos_scan();
}
