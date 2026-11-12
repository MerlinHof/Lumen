static void improvements_tests(void) {
    editor_ui.clip_active = 0;
    editor_ui.keyboard = 0;
    route_to(27);
    char expression[1024];
    memset(expression,'i',70);expression[70]=0;
    assert(package_line_start(expression,70,468)==0);
    memset(expression,'W',100);expression[100]=0;
    assert(package_line_start(expression,100,468)>0);
    assert(package_line_start(expression,1,468)==0);
    assert(!app_route_valid(116) && !app_route_valid(121));
    int ids[APP_COUNT], n = v1_visible(ids);
    for (int i = 0; i < n; i++)
        assert(ids[i] != APP_FILES && ids[i] != APP_RPG);
    memset(ink_pixels, 255, sizeof(ink_pixels));
    ink_eraser = 0;
    ink_width = 3;
    ink_stamp(100, 154);
    assert(ink_pixels[100 * 600 + 100] < 255);
    ink_eraser = 1;
    ink_stamp(100, 154);
    for (int y = 96; y <= 104; y++)
        for (int x = 96; x <= 104; x++)
            assert(ink_pixels[y * 600 + x] == 255);
    ink_note = 12345;
    ink_page = 0;
    ink_dirty = 1;
    ink_save();
    char p[2048];
    snprintf(p, sizeof(p), "%s/notes/12345-page-0.ink", data_root);
    assert(access(p, F_OK));
    ink_note = -1;
    route_to(1);
    open_keyboard(0, "Search", "select these words");
    ui_draw();
    Input h = {.kind = INPUT_HOLD, .x = 28 + textwidth_raw("select ", &body), .y = 476};
    ui_input(&h);
    assert(editor_ui.clip_active == 1);
    Input m = {.kind = INPUT_MOVE, .x = 28 + textwidth_raw("select these", &body), .y = 476};
    ui_input(&m);
    assert(editor_ui.clip_active == 1 && clip_e > clip_s);
    Input release = m;
    release.kind = INPUT_TAP;
    ui_input(&release);
    assert(editor_ui.clip_active == 2);
    Input copy = {.kind = INPUT_TAP, .x = 100, .y = 380};
    ui_input(&copy);
    assert(!strcmp(clipboard_text, "these"));
    editor_ui.keyboard = 0;
    route_to(114);
    companion_page16 = 2;
    puts("PASS improvements: pixel-measured formula scrolling, removed routes, real erasing, drag "
         "selection, recipe back.");
}
