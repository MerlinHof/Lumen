/* App-owned module; assembled by apps/host/compose.h. */
static void reader(void) {
    if (selected < 0)
        return;
    pagination_bind(&page, engine_count(), 0, 0, turn);
    if (!engine_render(page, ui_pixels + 12 * WIDTH + 28, WIDTH, 544, 728, zoom, panx, pany))
        ui_message("Dieses Buch konnte nicht geöffnet oder dargestellt werden.");
    annotations_draw();
    if (rounding_profile() && engine_reflowable()) {
        TextRect images[64];
        int n = engine_image_rects(images, 64);
        for (int i=0; i<n; i++) {
            TextRect r = images[i];
            if (r.x0 < 0 || r.y0 < 0 || r.x1 > 544 || r.y1 > 728) continue;
            int x = (int)ceilf(r.x0), y = (int)ceilf(r.y0);
            rounded_clip_apply(28+x,12+y,(int)floorf(r.x1)-x,(int)floorf(r.y1)-y,255);
        }
    }
    rect(0, 744, 600, 56, 255);
    if (!reader_ui.reader_chrome)
        u15_reader_progress();
    if (reader_ui.reader_chrome) {
        rect(0, 48, 600, READER_TOOLBAR_END - 48, 255);
        label_ellipsis(28, 74, 422, 44, library[selected].title, ui_font(UI_HEADING), 0);
        label_ellipsis(29, 74, 421, 44, library[selected].title, ui_font(UI_HEADING), 0);
        divider(0, READER_TOOLBAR_END - 2, 600);
        icon_button(READER_CONTENTS_X, 74, IC_CONTENTS);
        icon_button(READER_SETTINGS_X, 74, IC_SETTINGS);
    }
}
