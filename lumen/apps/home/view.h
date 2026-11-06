/* The Home illustration is rendered once per display theme. The date stays live. */
enum { HOME_ART_Y = 48, HOME_ART_H = 344, HOME_GRID_Y = 408, HOME_TILE_H = 132 };
static unsigned char home_art[WIDTH * HOME_ART_H];
static int home_art_theme = -1, home_page;

static void home_landscape(void) {
    if (home_art_theme != options[OPT_INVERT]) {
        home_art_theme = options[OPT_INVERT];
        aa_arc_color(477,156,32,180,360,1,226);
        aa_arc_color(477,156,32,0,180,1,226);
        for (int i = 0; i < 16; i++) {
            int x = (i*113+9)%WIDTH, y = 160+(i*23)%73;
            stroke_color(x,y,x+18+i%37,y-2,1,225+i%15);
        }
        /* Distant hills and shore, then pencil hatching in the foreground. */
        const int ridges[][9] = {
            {304,272,291,248,274,238,290,254,294},
            {326,305,315,282,310,267,311,287,307},
            {357,329,348,319,340,334,321,343,325}
        };
        for (int layer = 0; layer < 3; layer++) {
            int old_x = 0, old_y = ridges[layer][0];
            for (int x = 5; x < WIDTH; x += 5) {
                int segment = x / 75, fraction = x % 75;
                int y = (ridges[layer][segment]*(75-fraction) +
                         ridges[layer][segment+1]*fraction)/75;
                y += (int)(3*sinf(x*.109f+layer) + 2*sinf(x*.041f));
                stroke_color(old_x, old_y, x, y, 2, 175-layer*18);
                if (x % 10 == 0)
                    stroke_color(x, y+3, x-11-layer*2, y+17+layer*3, 1, 206-layer*10);
                old_x = x;
                old_y = y;
            }
        }
        for (int i = 0; i < 36; i++) {
            int x = (i*179+31)%WIDTH, y = 306+(i*19)%74;
            stroke_color(x, y, x+12+i%23, y, 1, 202+i%38);
        }
        /* Reeds on the near shore. Keep the date and the lake's center quiet. */
        for (int i = 0; i < 25; i++) {
            int x = i < 12 ? 8+i*8 : 499+(i-12)*8, y = 367+(i*7)%12;
            int h = 8+(i*13)%23, bend = (i*7)%17-8;
            stroke_color(x,y,x+bend,y-h,2,141+i%38);
            stroke_color(x+bend/2,y-h/2,x+bend+5,y-h-2,1,162);
        }
        int paper = theme_gray(255);
        for (int y = 0; y < HOME_ART_H; y++) {
            int fade = y < 52 ? y : HOME_ART_H-1-y;
            if (fade > 52) fade = 52;
            for (int x = 0; x < WIDTH; x++) {
                int ink = ui_pixels[(HOME_ART_Y+y)*WIDTH+x];
                home_art[y*WIDTH+x] = paper + (ink-paper)*fade/52;
            }
        }
    }
    memcpy(ui_pixels+HOME_ART_Y*WIDTH, home_art, sizeof(home_art));
}
static int home_apps(int *ids) {
    const AppId first[] = {APP_LIBRARY, APP_CALCULATOR, APP_NOTES, APP_CALENDAR,
                           APP_TASKS, APP_TIMER, APP_WEATHER, APP_SETTINGS};
    int n = 0, seen[APP_COUNT] = {0};
    for (unsigned i = 0; i < sizeof(first)/sizeof(*first); i++) {
        ids[n++] = first[i];
        seen[first[i]] = 1;
    }
    for (int i = 0; i < APP_COUNT; i++) {
        int app = launcher_order[i];
        if (app != APP_HOME && !seen[app] && tab_eligible(app))
            ids[n++] = app;
    }
    return n;
}
static void home_draw(void) {
    home_landscape();
    static const char *weekdays[] = {"Sonntag", "Montag", "Dienstag", "Mittwoch",
                                     "Donnerstag", "Freitag", "Samstag"};
    static const char *months[] = {"Januar","Februar","März","April","Mai","Juni",
                                   "Juli","August","September","Oktober","November","Dezember"};
    time_t now = time(NULL);
    struct tm day;
    localtime_r(&now, &day);
    char date[80];
    if (options[OPT_LANGUAGE] == 1)
        snprintf(date,sizeof(date),"%s %d, %d",tr(months[day.tm_mon]),day.tm_mday,day.tm_year+1900);
    else
        snprintf(date,sizeof(date),"%d%s %s %d",day.tm_mday,options[OPT_LANGUAGE] == 0 ? "." : "",
                 tr(months[day.tm_mon]),day.tm_year+1900);
    centered(28, 89, 544, 68, weekdays[day.tm_wday], &title);
    centered(28, 155, 544, 48, date, &editorial);

    int ids[APP_COUNT], n = home_apps(ids);
    for (int i = 0; i < 8 && home_page*8+i < n; i++) {
        int x = 20 + i%4*140, y = HOME_GRID_Y + i/4*HOME_TILE_H;
        app_tile(x, y, 140, ids[home_page*8+i]);
    }
    pager(682, &home_page, (n+7)/8);
}
static int home_input(const Input *in) {
    if (in->kind != INPUT_TAP || in->x < 20 || in->x >= 580 ||
        in->y < HOME_GRID_Y || in->y >= HOME_GRID_Y+2*HOME_TILE_H)
        return -1;
    int ids[APP_COUNT], n = home_apps(ids);
    int i = home_page*8 + (in->y-HOME_GRID_Y)/HOME_TILE_H*4 + (in->x-20)/140;
    if (i < n) {
        AppIntent intent = {.app = ids[i], .kind = INTENT_OPEN};
        app_send(&intent);
    }
    return 1;
}
