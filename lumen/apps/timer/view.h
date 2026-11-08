/* Stable saved face IDs; presentation order is independent of persistence. */
static const char *clock_styles[] = {"Klassisch · Ziffern", "Klassisch", "Minimal", "Orbit", "Blütenzeit", "Digital", "Digital · Dunkel"};
static const int clock_style_ids[] = {4,0,1,5,6,2,3};
static int clock_style_row(void) {
    for (int i=0;i<7;i++) if (clock_style_ids[i]==clock_ui.clock_style) return i;
    return 0;
}
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
    int cy = fullscreen ? 382 : 435;
    int r = fullscreen ? 220 : 168;
    int style = clock_ui.clock_style;
    int dark = style == 3;
    if (style == 2 || dark) {
        if (dark && fullscreen) rect(0,0,600,800,0);
        else if (dark) control_surface(54,cy-91,492,182,RADIUS_LARGE,0);
        clock_digits(cy,t.tm_hour,t.tm_min,dark ? 255 : 0);
    } else {
        if (style == 0 || style == 4) {
            ring(300,cy,r,2);
            ring(300,cy,r-7,1);
        }
        for (int i=0; i<60; i++) {
            double a = i * .104719755;
            if ((style == 1 || style >= 5) && i % 5) continue;
            int primary = !(i%15);
            int inner = r - (i%5 ? 11 : primary ? 24 : 19);
            int outer = r-9;
            if (style == 5) {
                aa_disc(300+sin(a)*(r-13),cy-cos(a)*(r-13),primary ? 6 : 3,primary ? 0 : 100);
            } else if (style == 6) {
                int cx=300+sin(a)*(r-17), yy=cy-cos(a)*(r-17);
                ui_paint_ellipse(cx,yy,primary ? 7 : 4,primary ? 7 : 4,primary ? 0 : 112);
                aa_disc(cx,yy,2,255);
            } else {
                aa_stroke(300+sin(a)*inner,cy-cos(a)*inner,
                          300+sin(a)*outer,cy-cos(a)*outer,primary ? 4 : 2);
            }
            if (style == 4 && !(i%5)) {
                char number[8];
                snprintf(number,sizeof(number),"%d",i ? i/5 : 12);
                centered(270+sin(a)*(r-47),cy-20-cos(a)*(r-47),60,40,number,ui_font(UI_BODY));
            }
        }
        double minute=t.tm_min*.104719755, hour=(t.tm_hour+t.tm_min/60.)*.523598776;
        /* Rounded, counterbalanced hands with a quiet open hub. */
        aa_stroke(300-sin(hour)*16,cy+cos(hour)*16,
                  300+sin(hour)*r*.48,cy-cos(hour)*r*.48,8);
        aa_stroke(300-sin(minute)*21,cy+cos(minute)*21,
                  300+sin(minute)*r*.74,cy-cos(minute)*r*.74,4);
        aa_disc(300,cy,8,0);
        aa_disc(300,cy,3,255);
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
    const char *tabs[] = {"Uhr", "Timer", "Wecker"};
    segments(28, 74, 544, 44, tabs, 3, timer_panel);
    char b[80];
    if (!timer_panel) {
        dropdown_button(28, 137, 310, 44, clock_styles[clock_style_row()]);
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
        /* One deliberate alarm card: time, adjustment and armed state together. */
        key_outline(28,194,544,408);
        icon_sized(52,216,IC_CLOCK,32);
        label_at(98,210,420,44,tr("Weckzeit"),ui_font(UI_BODY));
        clock_digits(370,alarm_hour,alarm_minute,0);
        icon_button(170,258,IC_UP);
        icon_button(395,258,IC_UP);
        icon_button(170,443,IC_DOWN);
        icon_button(395,443,IC_DOWN);
        divider(52,506,496);
        label_at(54,528,390,48,tr("Wecker aktiv"),ui_font(UI_PRIMARY));
        toggle(470,536,alarm_enabled);
        wrapped(30,620,"Einmaliger Wecker zur eingestellten Uhrzeit.",ui_font(UI_BODY),540,2);

    }
    wrapped(30, 706, "Visuelles Signal beim Lesen. Im Tiefschlaf kein Wecksignal.", ui_font(UI_SECONDARY), 540,
            3);
}
