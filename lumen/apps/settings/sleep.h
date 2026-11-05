/* App-owned module; assembled by apps/host/compose.h. */
static void u17_sleep_row(int y) {
    int i = 1;
    for (int j = 0; j < 6; j++)
        if (auto_sleep_minutes == sleep_minutes16[j])
            i = j;
    select_row17(y, "Ruhezustand", sleep_choices16[i]);
}
static void u17_sleep_select(int y) {
    int i = 1;
    for (int j = 0; j < 6; j++)
        if (auto_sleep_minutes == sleep_minutes16[j])
            i = j;
    open_dropdown(917, sleep_choices16, 6, i, 300, y + 53, 272);
}
