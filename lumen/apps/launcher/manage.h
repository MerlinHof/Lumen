/* App-owned module; assembled by apps/host/compose.h. */
static int launcher_manage_indices(int *indices) {
    int n = 0;
    for (int i = 0; i < APP_COUNT; i++)
        if (launcher_order[i] != APP_HOME && app_route_valid(app_definition(launcher_order[i])->entry))
            indices[n++] = i;
    return n;
}
static void v1_launcher_settings(void) {
    heading("Apps verwalten");
    drawtext(30, 127, "Sichtbarkeit & Reihenfolge", ui_font(UI_SECONDARY), 540);
    int indices[APP_COUNT], count = launcher_manage_indices(indices);
    for (int r = 0; r < 7 && launcher_settings_page * 7 + r < count; r++) {
        int pos = launcher_settings_page * 7 + r, i = indices[pos], id = launcher_order[i],
            y = 174 + r * 68;
        app_icon(28, y + 10, id);
        label_at(87, y + 10, 252, 44, tr(app_definition(id)->name), ui_font(UI_BODY));
        page_arrow(354, y + 10, IC_UP, pos > 0);
        page_arrow(405, y + 10, IC_DOWN, pos + 1 < count);
        toggle(492, y + 16, launcher_visible[id]);
        divider(28, y + 67, 544);
    }
    pager(745, &launcher_settings_page, (count + 6) / 7);
}
