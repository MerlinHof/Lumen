/* App-owned module; assembled by apps/host/compose.h. */
static void clock_digits(int cy, int hour, int minute, int color) {
    static const unsigned char digit[] = {63,6,91,79,102,109,125,7,127,111};
    static const int offsets[] = {109,196,334,421};
    int values[] = {hour/10,hour%10,minute/10,minute%10};
    for (int i=0; i<4; i++) {
        int x=offsets[i], y=cy-65, bits=digit[values[i]];
        if (bits&1)  control_surface(x+9,y,64,10,5,color);
        if (bits&2)  control_surface(x+69,y+10,10,50,5,color);
        if (bits&4)  control_surface(x+69,y+70,10,50,5,color);
        if (bits&8)  control_surface(x+9,y+120,64,10,5,color);
        if (bits&16) control_surface(x+3,y+70,10,50,5,color);
        if (bits&32) control_surface(x+3,y+10,10,50,5,color);
        if (bits&64) control_surface(x+9,y+60,64,10,5,color);
    }
    aa_disc(300,cy-20,7,color);
    aa_disc(300,cy+20,7,color);
}
static void clock_face(int fullscreen) {
    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);
    int cy = fullscreen ? 400 : 449;
    int r = fullscreen ? 220 : 168;
    int style = clock_ui.clock_style;
    int dark = style == 3;
    if (style == 2 || dark) {
        if (dark && fullscreen) rect(0,0,600,800,0);
        else if (dark) control_surface(54,cy-91,492,182,RADIUS_LARGE,0);
        clock_digits(cy,t.tm_hour,t.tm_min,dark ? 255 : 0);
    } else {
        if (style != 1) ring(300,cy,r,3);
        for (int i=0; i<60; i++) {
            double a = i * .104719755;
            if (style == 1 && i % 5) continue;
            int inner = r - (i%5 ? 12 : 23);
            int primary = i % (style == 1 ? 15 : 5) == 0;
            if (style == 1 && !primary) inner = r - 12;
            stroke_color(300+sin(a)*inner, cy-cos(a)*inner,
                         300+sin(a)*(r-5), cy-cos(a)*(r-5), primary ? 4 : 2,
                         style == 1 && !primary ? 112 : 0);
            if (style == 4 && !(i%5)) {
                char label[8];
                snprintf(label,sizeof(label),"%d",i ? i/5 : 12);
                centered(270+sin(a)*(r-47), cy-20-cos(a)*(r-47),60,40,label,ui_font(UI_BODY));
            }
        }
        double minute=t.tm_min*.104719755, hour=(t.tm_hour+t.tm_min/60.)*.523598776;
        stroke(300,cy,300+sin(hour)*r*.5,cy-cos(hour)*r*.5,style==1 ? 6 : 8);
        stroke(300,cy,300+sin(minute)*r*.69,cy-cos(minute)*r*.69,4);
        aa_disc(300,cy,6,0);
    }
    char date[32];
    strftime(date,sizeof(date),"%d.%m.%Y",&t);
    int ink = ink_color;
    ink_color = dark && fullscreen ? 255 : UI_SECONDARY_INK;
    centered(60,cy+r+14,480,48,date,ui_font(UI_SECONDARY));
    ink_color = ink;
}
static void clock_show_face(void) { timer_panel = 0; }
static void app_timer(void) {
    heading_width("Uhr", 128);
    const char *tabs[] = {"Uhr", "Timer", "Wecker"};
    segments(166, 74, 406, 44, tabs, 3, timer_panel);
    char b[80];
    if (!timer_panel) {
        static const char *styles[] = {"Klassisch", "Minimal", "Digital", "Digital · Dunkel", "Klassisch · Ziffern"};
        dropdown_button(28, 137, 250, 44, styles[clock_ui.clock_style]);
        icon_button(528, 137, IC_FULLSCREEN);
        clock_face(0);
        return;
    }
    if (timer_panel == 1) {
        extra_timer_label(b, sizeof(b));
        if (!*b)
            snprintf(b, sizeof(b), "%02d:00", timer_minutes);
        v1_timer_ring(0);
        icon_button(528, 185, IC_FULLSCREEN);
        const int presets[] = {5, 10, 20, 30};
        for (int i = 0; i < 4; i++) {
            snprintf(b, sizeof(b), "%d min", presets[i]);
            button_h(28 + i * 140, 397, 124, 48, b, !timer_due && timer_minutes == presets[i]);
        }
        button_h(28, 472, 80, 52, "−", 0);
        snprintf(b, sizeof(b), "%d min", timer_minutes);
        button_h(122, 472, 356, 52, b, 0);
        button_h(492, 472, 80, 52, "+", 0);
        action_button(28, 566, 260, 64,
                 timer_due      ? "Pause"
                 : timer_paused ? "Fortsetzen"
                                : "Timer starten");
        button_h(312, 566, 260, 64, "Zurücksetzen", 0);
    } else {
        centered(28, 225, 544, 36, "Weckzeit", ui_font(UI_SECONDARY));
        snprintf(b, sizeof(b), "%02d:%02d", alarm_hour, alarm_minute);
        type_centered(100, 265, 400, 104, b, UI_DISPLAY);
        button_h(153, 380, 294, 48, "Weckzeit einstellen", 0);
        label_at(30, 442, 430, 48, tr("Wecker aktiv"), ui_font(UI_PRIMARY));
        toggle(492, 450, alarm_enabled);
        wrapped(30, 535, "Einmaliger Wecker zur eingestellten Uhrzeit.", ui_font(UI_BODY), 540, 3);
    }
    wrapped(30, 664, "Visuelles Signal beim Lesen. Im Tiefschlaf kein Wecksignal.", ui_font(UI_SECONDARY), 540,
            3);
}
