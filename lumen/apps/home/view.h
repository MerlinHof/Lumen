enum { HOME_ART_Y = 56, HOME_ART_H = 336, HOME_GRID_Y = 408,
       HOME_FULL_GRID_Y = 80, HOME_TILE_H = 126, HOME_PAGER_Y = 744 };
static unsigned char home_art[WIDTH*HOME_ART_H];
static int home_scene = -1, home_page;

static void home_landscape(time_t now) {
    /* A stable pseudo-random scene per local day avoids flicker during redraws. */
    unsigned day = (unsigned)app_date_key(now);
    day ^= day >> 16;
    day *= 0x7feb352du;
    day ^= day >> 15;
    int scene = day % 7;
    if (scene != home_scene) {
        char path[2048];
        snprintf(path,sizeof(path),"%s/../home/scene-%d.pgm",data_root,scene);
        FILE *f = fopen(path,"rb");
#ifdef LUMEN_HOST
        if (!f) {
            snprintf(path,sizeof(path),"lumen/assets/home/scene-%d.pgm",scene);
            f = fopen(path,"rb");
        }
#endif
        char header[16];
        const char expected[] = "P5\n600 336\n255\n";
        if (!f || fread(header,1,sizeof(expected)-1,f) != sizeof(expected)-1 ||
            memcmp(header,expected,sizeof(expected)-1) ||
            fread(home_art,1,sizeof(home_art),f) != sizeof(home_art))
            memset(home_art,255,sizeof(home_art));
        if (f) fclose(f);
        home_scene = scene;
    }
    memcpy(ui_pixels+HOME_ART_Y*WIDTH,home_art,sizeof(home_art));
}
static int home_favorites(int *ids) {
    const AppId first[] = {APP_LIBRARY, APP_CALCULATOR, APP_NOTES, APP_CALENDAR,
                           APP_TASKS, APP_TIMER, APP_WEATHER, APP_SETTINGS};
    int n = 0, seen[APP_COUNT] = {0};
    for (unsigned i=0; i<sizeof(first)/sizeof(*first); i++)
        if (launcher_visible[first[i]]) {
            ids[n++] = first[i];
            seen[first[i]] = 1;
        }
    int apps[APP_COUNT], total = v1_visible(apps);
    for (int i=0; i<total && n<8; i++)
        if (!seen[apps[i]]) ids[n++] = apps[i];
    return n;
}
static void home_date(void) {
    static const char *weekdays[] = {"Sonntag","Montag","Dienstag","Mittwoch",
                                     "Donnerstag","Freitag","Samstag"};
    static const char *months[] = {"Januar","Februar","März","April","Mai","Juni",
                                   "Juli","August","September","Oktober","November","Dezember"};
    time_t now = time(NULL);
    home_landscape(now);
    struct tm day;
    localtime_r(&now,&day);
    char date[80];
    if (options[OPT_LANGUAGE] == 1)
        snprintf(date,sizeof(date),"%s %d, %d",tr(months[day.tm_mon]),day.tm_mday,day.tm_year+1900);
    else
        snprintf(date,sizeof(date),"%d%s %s %d",day.tm_mday,options[OPT_LANGUAGE] == 0 ? "." : "",
                 tr(months[day.tm_mon]),day.tm_year+1900);
    centered(28,89,544,68,weekdays[day.tm_wday],&title);
    centered(28,155,544,48,date,&editorial);
}
static void home_draw(void) {
    polish_order();
    int ids[APP_COUNT], n = v1_visible(ids), pages = 1+(n+19)/20;
    if (home_page >= pages) home_page = pages-1;
    int top = HOME_FULL_GRID_Y, start = (home_page-1)*20, limit = 20;
    if (home_page == 0) {
        home_date();
        top = HOME_GRID_Y;
        start = 0;
        limit = 8;
        n = home_favorites(ids);
    }
    for (int i=0; i<limit && start+i<n; i++)
        app_tile(20+i%4*140,top+i/4*HOME_TILE_H,140,ids[start+i]);
    if (!launcher_visible[APP_SETTINGS])
        icon_button(528,56,IC_SETTINGS);
    pager(HOME_PAGER_Y,&home_page,pages);
}
static int home_input(const Input *in) {
    if (in->kind != INPUT_TAP) return -1;
    if (!launcher_visible[APP_SETTINGS] && in->x >= 528 && in->y >= 56 && in->y < 100) {
        AppIntent intent = {.app=APP_SETTINGS,.kind=INTENT_OPEN};
        app_send(&intent);
        return 1;
    }
    int top = home_page ? HOME_FULL_GRID_Y : HOME_GRID_Y;
    int rows = home_page ? 5 : 2;
    if (in->x < 20 || in->x >= 580 || in->y < top || in->y >= top+rows*HOME_TILE_H)
        return -1;
    int ids[APP_COUNT], n = home_page ? v1_visible(ids) : home_favorites(ids);
    int i = (home_page ? (home_page-1)*20 : 0) +
            (in->y-top)/HOME_TILE_H*4 + (in->x-20)/140;
    if (i < n) {
        AppIntent intent = {.app=ids[i],.kind=INTENT_OPEN};
        app_send(&intent);
    }
    return 1;
}
