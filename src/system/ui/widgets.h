/* System widgets shared by applications; hit regions and text metrics stay consistent. */
static void label_at(int x, int y, int w, int h, const char *s, const Font *f) {
    int height, top = ink_top(s, f, &height);
    drawtext_raw(x, y + (h - height) / 2 - top, s, f, w);
}
static void search_field(int x, int y, int w, const char *value, const char *hint) {
    region(x, y, w, 48);
    key_outline(x, y, w, 48);
    icon(x + 6, y + 2, IC_SEARCH);
    ink_color = *value ? 0 : 112;
    int h, t = ink_top(*value ? value : tr(hint), &small, &h);
    drawtext_raw(x + 55, y + (48 - h) / 2 - t, *value ? value : tr(hint), &small, w - 70);
    ink_color = 0;
}

/* Search lives in the title bar; the shared keyboard carries the query. */
static void search_action(void) { icon_button(528, 74, IC_SEARCH); }
static int search_action_hit(int x, int y) { return x >= 516 && x < 580 && y >= 74 && y < 118; }
