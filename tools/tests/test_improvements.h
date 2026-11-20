static void improvements_tests(void) {
    editor_ui.clip_active = 0;
    editor_ui.keyboard = 0;
    activate_app(app_package_find("lumen.calculator"));
    char expression[1024];
    memset(expression,'i',70);expression[70]=0;
    assert(package_line_start(expression,70,468)==0);
    memset(expression,'W',100);expression[100]=0;
    assert(package_line_start(expression,100,468)>0);
    assert(package_line_start(expression,1,468)==0);
    assert(!app_package_foreground(app_package_find("retired.files")) && !app_package_foreground(app_package_find("retired.rpg")));
    int ids[APP_LIMIT], n = v1_visible(ids);
    for (int i = 0; i < n; i++)
        assert(app_package_foreground(ids[i]));
    activate_app(app_package_find("lumen.library"));
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
    Input copy = {.kind = INPUT_TAP, .x = 100, .y = clipboard_menu_y()+64};
    ui_input(&copy);
    assert(!strcmp(clipboard_text, "these"));
    editor_ui.keyboard = 0;
    puts("PASS improvements: pixel-measured formula scrolling, removed routes, real erasing, drag "
         "selection.");
}
