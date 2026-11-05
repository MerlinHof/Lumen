static void tabbar(void) {
    rect(0, 736, 600, 64, 255);
    divider(screen == 2 ? 0 : 28, 736, screen == 2 ? 600 : 544);
    int count = options[OPT_APPS] ? 4 : 3, width = 600 / count;
    int active = app_definition(app_for_route(screen))->tab;
    const int kinds[] = {IC_BOOK, IC_SETTINGS, IC_STORE, IC_GRID};
    const char *labels[] = {"Bibliothek", "Einstellungen", "Store", "Apps"};
    for (int i = 0; i < count; i++) {
        int x = i * width;
        system_region(x, 737, width, 63);
        icon(x + width / 2 - 22, options[OPT_TAB_LABELS] ? 742 : 748, kinds[i]);
        if (options[OPT_TAB_LABELS])
            centered(x, 779, width, 20, labels[i], &caption);
        if (i == active)
            rect(x + width / 2 - 28, 736, 56, 4, 0);
    }
}
