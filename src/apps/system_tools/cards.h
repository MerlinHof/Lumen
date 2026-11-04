/* App-owned module; assembled by apps/host/compose.h. */
static void stat_card(int x, int y, const char *label, const char *value) {
    key_outline(x, y, 260, 108);
    label_at(x + 15, y + 12, 230, 24, tr(label), ui_font(UI_SECONDARY));
    label_ellipsis(x + 15, y + 44, 230, 44, tr(value), ui_font(UI_TITLE), 0);
}
static void app_restart(void) {
    icon_button(528, 74, IC_RESTART);
}
