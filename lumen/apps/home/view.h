enum { HOME_ART_Y = 48, HOME_ART_H = 404,
       HOME_FULL_GRID_Y = 80, HOME_TILE_H = 126,
       HOME_GRID_Y = HOME_FULL_GRID_Y + 3*HOME_TILE_H, HOME_PAGER_Y = 744 };
static unsigned char home_art[WIDTH*HOME_ART_H];
static int home_page;

static void home_landscape(void) {
    static int loaded;
    if (!loaded) {
        char path[2048];
        snprintf(path,sizeof(path),"%s/../home/scene-0.pgm",data_root);
        FILE *f = fopen(path,"rb");
#ifdef LUMEN_HOST
        if (!f) f=fopen("lumen/assets/home/scene-0.pgm","rb");
#endif
        char header[16];
        const char expected[] = "P5\n600 404\n255\n";
        if (!f || fread(header,1,sizeof(expected)-1,f) != sizeof(expected)-1 ||
            memcmp(header,expected,sizeof(expected)-1) ||
            fread(home_art,1,sizeof(home_art),f) != sizeof(home_art))
            memset(home_art,255,sizeof(home_art));
        if (f) fclose(f);
        loaded=1;
    }
    memcpy(ui_pixels+HOME_ART_Y*WIDTH,home_art,sizeof(home_art));
}
static void home_date(void) {
    static const char *weekdays[] = {"Sonntag","Montag","Dienstag","Mittwoch",
                                     "Donnerstag","Freitag","Samstag"};
    static const char *months[] = {"Januar","Februar","März","April","Mai","Juni",
                                   "Juli","August","September","Oktober","November","Dezember"};
    time_t now = time(NULL);
    home_landscape();
    struct tm day;
    localtime_r(&now,&day);
    char date[80], year[12];
    snprintf(year,sizeof(year),"%d",day.tm_year+1900);
    if (options[OPT_LANGUAGE] == 1)
        snprintf(date,sizeof(date),"%s %d",tr(months[day.tm_mon]),day.tm_mday);
    else
        snprintf(date,sizeof(date),"%d%s %s",day.tm_mday,options[OPT_LANGUAGE] == 0 ? "." : "",
                 tr(months[day.tm_mon]));
    centered(108,111,384,40,weekdays[day.tm_wday],&editorial);
    centered(96,150,408,56,date,&title);
    rect(235,224,32,1,118);
    rect(333,224,32,1,118);
    centered(267,207,66,36,year,&caption);
}
static void home_draw(void) {
    int ids[APP_COUNT], n = v1_visible(ids), pages = 1+(n>8 ? (n-8+19)/20 : 0);
    if (home_page >= pages) home_page = pages-1;
    int top = HOME_FULL_GRID_Y, start = 8+(home_page-1)*20, limit = 20;
    if (home_page == 0) {
        home_date();
        top = HOME_GRID_Y;
        start = 0;
        limit = 8;
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
    int ids[APP_COUNT], n = v1_visible(ids);
    int i = (home_page ? 8+(home_page-1)*20 : 0) +
            (in->y-top)/HOME_TILE_H*4 + (in->x-20)/140;
    if (i < n) {
        AppIntent intent = {.app=ids[i],.kind=INTENT_OPEN};
        app_send(&intent);
        if (intent.app == APP_HOME) home_page = 0;
    }
    return 1;
}
