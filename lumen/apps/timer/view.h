/* Stable IDs preserve saved preferences; this order follows visual families. */
enum { CLOCK_FACE_COUNT=14 };
static const char *clock_styles[] = {
    "Klassisch · Ziffern", "Klassisch", "Römisch", "Bahnhof", "Minimal", "Orbit",
    "Bauhaus", "Sonnenuhr · 24 h", "Kompass", "Blütenzeit", "Typografie", "Editorial", "Stunde für Stunde", "Nacht"};
static const int clock_style_ids[] = {4,0,7,8,1,5,9,12,13,6,2,10,11,3};
static int clock_style_row(void) {
    for(int i=0;i<CLOCK_FACE_COUNT;i++)if(clock_style_ids[i]==clock_ui.clock_style)return i;
    return 0;
}
static void clock_hand(int cy,double angle,int length,int tail,int width,int color) {
    stroke_color(300-sin(angle)*tail,cy+cos(angle)*tail,
                 300+sin(angle)*length,cy-cos(angle)*length,width,color);
}
static void clock_marker(int cy,int r,double a,int length,int weight,int gray) {
    stroke_color(300+sin(a)*(r-length),cy-cos(a)*(r-length),
                 300+sin(a)*(r-5),cy-cos(a)*(r-5),weight,gray);
}
static void clock_face(int fullscreen) {
    time_t now=time(NULL);struct tm t;localtime_r(&now,&t);
    int cy=fullscreen ? 382 : 435, r=fullscreen ? 220 : 168;
    int style=clock_ui.clock_style,dark=style==3;
    char value[32];
    if(style==2 || style==3 || style==10 || style==11) {
        if(dark) {
            if(fullscreen)rect(0,0,600,800,0);
            else control_surface(28,cy-110,544,220,RADIUS_LARGE,0);
        }
        snprintf(value,sizeof(value),"%02d:%02d",t.tm_hour,t.tm_min);
        if(style==11) {
            snprintf(value,sizeof(value),"%02d",t.tm_hour);
            type_display(70,cy-150,460,142,value,1,144,0);
            snprintf(value,sizeof(value),"%02d",t.tm_min);
            type_display(70,cy+6,460,142,value,1,144,0);
            divider(260,cy,80);
        } else {
            type_display(28,cy-85,544,170,value,style==2 ? 0 : 1,fullscreen ? 136 : 116,dark ? 255 : 0);
            if(style==10) {
                stroke_color(242,cy+117,358,cy+117,2,112);
                centered(90,cy-151,420,34,"L U M E N",ui_font(UI_SECONDARY));
            }
        }
    } else {
        if(style==0 || style==4 || style==7 || style==8) {
            ring(300,cy,r,style==8 ? 3 : 2);
            if(style!=8)ring(300,cy,r-7,1);
        }
        if(style==5)ring(300,cy,r*.7,2);
        if(style==12) {
            aa_arc_color(300,cy,r-10,180,360,3,0);
            aa_arc_color(300,cy,r-10,0,180,2,155);
            stroke_color(300-r+10,cy,300+r-10,cy,2,175);
        }
        for(int i=0;i<60;i++) {
            double a=i*.104719755;int quarter=!(i%15);
            if(style==12)continue;
            if((style==1 || style==5 || style==6 || style==9 || style==13) && i%5)continue;
            if(style==5)aa_disc(300+sin(a)*(r-12),cy-cos(a)*(r-12),quarter ? 6 : 3,quarter ? 0 : 104);
            else if(style==6) {
                /* A small engraved petal, rotated radially with its hour. */
                for(int j=0;j<32;j++) {
                    double u=j*.196349541,v=(j+1)*.196349541;
                    double ru=r-18+10*cos(u),rv=r-18+10*cos(v);
                    double tu=5*sin(u),tv=5*sin(v);
                    aa_stroke(300+sin(a)*ru+cos(a)*tu,cy-cos(a)*ru+sin(a)*tu,
                              300+sin(a)*rv+cos(a)*tv,cy-cos(a)*rv+sin(a)*tv,2);
                }
            } else if(style==9) {
                if(quarter)aa_disc(300+sin(a)*(r-14),cy-cos(a)*(r-14),8,0);
                else clock_marker(cy,r,a,12,3,112);
            } else clock_marker(cy,r,a,i%5 ? 11 : quarter ? 25 : 19,
                                style==8 ? (i%5 ? 3 : 6) : quarter ? 4 : 2,
                                style==13 && !quarter ? 128 : 0);
            if(!(i%5) && (style==4 || style==7)) {
                static const char *roman[]={"XII","I","II","III","IV","V","VI","VII","VIII","IX","X","XI"};
                if(style==7)snprintf(value,sizeof(value),"%s",roman[i/5]);
                else snprintf(value,sizeof(value),"%d",i ? i/5 : 12);
                centered(266+sin(a)*(r-49),cy-22-cos(a)*(r-49),68,44,value,
                         style==7 ? ui_font(UI_BODY) : ui_font(UI_PRIMARY));
            }
        }
        if(style==13) {
            const char *letters[]={"N","E","S","W"};
            for(int i=0;i<4;i++) {
                double a=i*1.570796327;
                centered(275+sin(a)*(r-43),cy-20-cos(a)*(r-43),50,40,letters[i],ui_font(UI_BODY));
            }
        }
        if(style==12) {
            const char *hours[]={"12","18","24","6"};
            for(int i=0;i<4;i++) {
                double a=i*1.570796327;
                centered(275+sin(a)*(r-38),cy-20-cos(a)*(r-38),50,40,hours[i],ui_font(UI_BODY));
            }
        }
        double minute=t.tm_min*.104719755;
        double hour=style==12 ? (t.tm_hour+t.tm_min/60.)*.261799388-3.141592654 :
                               (t.tm_hour+t.tm_min/60.)*.523598776;
        int hour_width=style==8 || style==9 ? 11 : style==1 ? 5 : 8;
        int minute_width=style==8 ? 6 : style==9 ? 5 : 3;
        clock_hand(cy,hour,r*.48,style==1 ? 0 : 15,hour_width,0);
        clock_hand(cy,minute,r*.75,style==1 ? 0 : 20,minute_width,0);
        if(style==8) {
            aa_disc(300+sin(minute)*r*.63,cy-cos(minute)*r*.63,9,0);
            aa_disc(300+sin(minute)*r*.63,cy-cos(minute)*r*.63,4,255);
        }
        if(style==5 || style==12)aa_disc(300+sin(hour)*r*.48,cy-cos(hour)*r*.48,7,0);
        aa_disc(300,cy,style==1 ? 5 : 8,0);
        if(style!=1)aa_disc(300,cy,3,255);
    }
    strftime(value,sizeof(value),"%d.%m.%Y",&t);
    int old_ink=ink_color;ink_color=dark && fullscreen ? 255 : UI_SECONDARY_INK;
    centered(60,cy+r+14,480,48,value,ui_font(UI_SECONDARY));ink_color=old_ink;
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
        snprintf(b,sizeof(b),"%02d:%02d",alarm_hour,alarm_minute);
        type_display(70,305,460,120,b,1,96,0);
        icon_button(216,258,IC_UP);
        icon_button(339,258,IC_UP);
        icon_button(216,443,IC_DOWN);
        icon_button(339,443,IC_DOWN);
        divider(52,506,496);
        label_at(54,528,390,48,tr("Wecker aktiv"),ui_font(UI_PRIMARY));
        toggle(470,536,alarm_enabled);
        wrapped(30,620,"Einmaliger Wecker zur eingestellten Uhrzeit.",ui_font(UI_BODY),540,2);

    }
    wrapped(30, 706, "Visuelles Signal beim Lesen. Im Tiefschlaf kein Wecksignal.", ui_font(UI_SECONDARY), 540,
            3);
}
