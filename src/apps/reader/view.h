/* App-owned module; assembled by apps/host/compose.h. */
static void reader(void) {
    if (selected < 0)
        return;
    pagination_bind(&page, engine_count(), 0, 0, turn);
    if (!engine_render(page, ui_pixels + 12 * WIDTH + 28, WIDTH, 544, 728, zoom, panx, pany))
        ui_message("Dieses Buch konnte nicht geöffnet oder dargestellt werden.");
    annotations_draw();
    rect(0, 744, 600, 56, 255);
    if (!reader_chrome)
        u15_reader_progress();
    if (reader_chrome) {
        rect(0, 48, 600, 116, 255);
        label_ellipsis(28, 56, 544, 36, library[selected].title, ui_font(UI_PRIMARY), 0);
        label_ellipsis(29, 56, 543, 36, library[selected].title, ui_font(UI_PRIMARY), 0);
        rect(0, 162, 600, 2, 190);

        region(65, 108, 100, 44);
        centered(65, 108, 100, 44, "Aa", ui_font(UI_PRIMARY));
        icon_button(278, 108, IC_CONTENTS);
        icon_button(478, 108, IC_SETTINGS);
    }
}
