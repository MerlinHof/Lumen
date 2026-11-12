/* App-owned module; assembled by apps/host/compose.h. */
static void new_apps_draw(void) {
    if (ui_route() >= 70) {
        v1_draw();
        return;
    }
    switch (ui_route()) {
    }
}
static int new_apps_commit(int target, const char *text) {
    if (target >= 100)
        return v1_commit(target, text);
    return 0;
}
static int new_apps_input(const Input *in) {
    if (ui_route() >= 70)
        return v1_input(in);
    if (ui_route() == 67 && in->kind == INPUT_SWIPE) {
        int total = newspaper.count + 2;
        if (in->dx < 0 && news_page + 1 < total)
            news_page++;
        else if (in->dx > 0 && news_page)
            news_page--;
        return 1;
    }
    if (in->kind != INPUT_TAP)
        return 2;

    return 1;
}
