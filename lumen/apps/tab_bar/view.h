/* Stable AppId values are saved in order. Empty/invalid files use the default. */
enum { TAB_LIMIT = 8, TAB_ROW_Y = 226, TAB_ROW_H = 62, TAB_ROWS = 7 };
typedef struct { int count, apps[TAB_LIMIT]; } TabLayout;
static const TabLayout default_tabs = {3,{APP_HOME,APP_LIBRARY,APP_LAUNCHER}};
static TabLayout tabs;
static int tab_settings_page;
static int tab_eligible(int app) {
    const AppDefinition *d = app_definition(app);
    return d && d->entry >= 0;
}
static void tabs_load(void) {
    tabs = default_tabs;
    TabLayout saved;
    if (!app_state_load("tab-bar-v1.bin", &saved, sizeof(saved)) ||
        saved.count < 1 || saved.count > TAB_LIMIT) return;
    int seen[APP_COUNT] = {0};
    for (int i = 0; i < saved.count; i++)
        if (!tab_eligible(saved.apps[i]) || seen[saved.apps[i]]++) return;
    tabs = saved;
}
static int tab_index(int app) {
    for (int i = 0; i < tabs.count; i++)
        if (tabs.apps[i] == app) return i;
    return -1;
}
static int tab_at(int x) {
    if (x < 0 || x >= WIDTH) return -1;
    for (int i = 0; i < tabs.count; i++)
        if (x < WIDTH*(i+1)/tabs.count) return tabs.apps[i];
    return -1;
}
static void tabbar(void) {
    rect(0, 736, 600, 64, 255);
    divider(ui_route() == 2 ? 0 : 28, 736, ui_route() == 2 ? 600 : 544);
    AppId current_app = app_for_route(ui_route());
    for (int i = 0; i < tabs.count; i++) {
        int x = WIDTH*i/tabs.count, w = WIDTH*(i+1)/tabs.count-x;
        system_region(x,737,w,63);
        widget_shape(x,737,w,63,RADIUS_MEDIUM,3,1);
        app_icon(x+w/2-22, options[OPT_TAB_LABELS] ? 739 : 748, tabs.apps[i]);
        if (options[OPT_TAB_LABELS])
            centered(x,778,w,20,app_definition(tabs.apps[i])->name,&caption);
        if ((int)current_app == tabs.apps[i])
            control_surface(x+w/2-24,736,48,4,2,0);
    }
}
static int tab_settings_apps(int *ids) {
    int n = 0;
    for (int i = 0; i < tabs.count; i++) ids[n++] = tabs.apps[i];
    for (int i = 0; i < APP_COUNT; i++)
        if (tab_eligible(i) && tab_index(i) < 0) ids[n++] = i;
    return n;
}
static void tab_settings_draw(void) {
    heading("Tableiste");
    form_label(28,130,430,48,"Tableiste anzeigen");
    toggle(492,138,options[OPT_TAB_BAR]);
    int ids[APP_COUNT], n = tab_settings_apps(ids);
    form_label(28,181,544,36,"Apps wählen und anordnen");
    for (int row = 0; row < TAB_ROWS && tab_settings_page*TAB_ROWS+row < n; row++) {
        int y = TAB_ROW_Y+row*TAB_ROW_H, app = ids[tab_settings_page*TAB_ROWS+row];
        int index = tab_index(app);
        app_icon(28,y+8,app);
        form_label(82,y+4,315,50,app_definition(app)->name);
        if (index > 0) icon_button(416,y+9,IC_UP);
        if (index >= 0 && index+1 < tabs.count) icon_button(470,y+9,IC_DOWN);
        icon_button(528,y+9,index >= 0 ? IC_CLOSE : IC_PLUS);
        divider(28,y+TAB_ROW_H-1,544);
    }
    pager(677,&tab_settings_page,(n+TAB_ROWS-1)/TAB_ROWS);
}
static int tab_settings_input(const Input *in) {
    if (in->kind != INPUT_TAP) return -1;
    if (in->x >= 480 && in->y >= 130 && in->y < 178) {
        options[OPT_TAB_BAR] ^= 1;
        library_options_save();
        return 1;
    }
    int row = (in->y-TAB_ROW_Y)/TAB_ROW_H;
    if (in->y < TAB_ROW_Y || row >= TAB_ROWS || in->x < 416 ||
        (in->y-TAB_ROW_Y)%TAB_ROW_H < 9 || (in->y-TAB_ROW_Y)%TAB_ROW_H >= 53) return 1;
    int ids[APP_COUNT], n = tab_settings_apps(ids), item = tab_settings_page*TAB_ROWS+row;
    if (item >= n) return 1;
    int app = ids[item], index = tab_index(app), x = in->x;
    if (x >= 528 && x < 572) {
        if (index >= 0 && tabs.count > 1) {
            memmove(tabs.apps+index,tabs.apps+index+1,(tabs.count-index-1)*sizeof(int));
            tabs.count--;
        } else if (index < 0 && tabs.count < TAB_LIMIT)
            tabs.apps[tabs.count++] = app;
        else {
            ui_message(index < 0 ? "Bis zu acht Apps in der Tableiste." : "Mindestens eine App auswählen.");
            return 1;
        }
    } else if (index > 0 && x < 460) {
        tabs.apps[index] = tabs.apps[index-1];
        tabs.apps[--index] = app;
        tab_settings_page = index/TAB_ROWS;
    } else if (index >= 0 && index+1 < tabs.count && x >= 470 && x < 514) {
        tabs.apps[index] = tabs.apps[index+1];
        tabs.apps[++index] = app;
        tab_settings_page = index/TAB_ROWS;
    } else return 1;
    app_state_save("tab-bar-v1.bin", &tabs, sizeof(tabs));
    return 1;
}
