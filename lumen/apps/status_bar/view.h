static void header(void) {
    time_t now = time(NULL);
    refresh_status(0);
    rect(0, 0, 600, STATUS_HEIGHT, 255);
    int left = can_back() ? 68 : 24;
    if (can_back()) {
        system_region(0, 0, 62, STATUS_HEIGHT);
        icon_sized(9, 6, IC_BACK, 44);
    }
    char date[32], clock[16], percent[24], timer[32];
    struct tm t;
    localtime_r(&now, &t);
    strftime(date, sizeof(date), "%d.%m.", &t);
    strftime(clock, sizeof(clock), "%H:%M", &t);
    snprintf(percent, sizeof(percent), "%d%%", atoi(status_ui.battery));
    status_ui.status_date_w = status_ui.status_clock_w = 0;
    int right = 581, previous = -1;
    status_ui.status_wifi_x = status_ui.status_battery_x = -1;
    for (int element = 0; element < 3; element++) {
        int enabled = element == 0   ? options[OPT_BATTERY]
                      : element == 1 ? options[OPT_BATTERY_PERCENT]
                      : options[OPT_WIFI];
        if (!enabled)
            continue;
        int gap = previous < 0 ? 0 : element == 1 ? 8 : 16;
        right -= gap;
        const char *label = element == 1 ? percent : date;
        int width = element == 0   ? 32
                    : element == 2 ? 30
                                   : textwidth(label, &small),
            x = right - width;
        if (element == 0) {
            status_ui.status_battery_x = x;
            status_ui.status_battery_right = right;
            control_surface(x, 19, 30, 18, 9, 0);
            control_surface(x + 2, 21, 26, 14, 7, 255);
            control_surface(x + 30, 26, 2, 4, 1, 0);
            int fill = 22 * atoi(status_ui.battery) / 100;
            if (fill > 22)
                fill = 22;
            if (fill > 0)
                control_surface(x + 4, 23, fill, 10, 5, 0);
            if(status_ui.battery_charging) {
                /* White halo separates the black bolt from every charge fill. */
                stroke_color(x+18,20,x+11,29,5,255);stroke_color(x+11,29,x+18,28,5,255);stroke_color(x+18,28,x+13,35,5,255);
                stroke(x+18,20,x+11,29,2);stroke(x+11,29,x+18,28,2);stroke(x+18,28,x+13,35,2);
            }
            system_region(status_ui.status_battery_x - 5, 0, 44, STATUS_HEIGHT);
        } else if (element == 2) {
            status_ui.status_wifi_x = x;
            wifi_symbol(x, 6);
            if (!strcmp(status_ui.wifi, "Offline"))
                aa_stroke(x + 2, 15, x + 27, 38, 2);
            system_region(x - 5, 0, 40, STATUS_HEIGHT);
        } else {
            centered(x - 8, 0, width + 16, STATUS_HEIGHT, label, &small);
            if (element == 1) {
                system_region(x - 4, 0, width + 8, STATUS_HEIGHT);
                if (status_ui.status_battery_x < 0 || x < status_ui.status_battery_x) {
                    status_ui.status_battery_x = x;
                    if (status_ui.status_battery_right < right || !options[OPT_BATTERY])
                        status_ui.status_battery_right = right;
                }
            }
        }
        right = x;
        previous = element;
    }
    if (options[OPT_CLOCK]) {
        int w = textwidth_raw(clock,&small);
        right -= previous < 0 ? 0 : 13;
        status_ui.status_clock_x = right-w-4;
        status_ui.status_clock_w = w+8;
        system_region(status_ui.status_clock_x,0,w+8,STATUS_HEIGHT);
        centered(right-w-8,0,w+16,STATUS_HEIGHT,clock,&small);
        right -= w;
        previous = 3;
    }
    if (options[OPT_DATE]) {
        int w = textwidth_raw(date,&small);
        right -= previous < 0 ? 0 : 12;
        status_ui.status_date_x = right-w-4;
        status_ui.status_date_w = w+8;
        system_region(status_ui.status_date_x,0,w+8,STATUS_HEIGHT);
        centered(right-w-8,0,w+16,STATUS_HEIGHT,date,&small);
        right -= w;
    }
    const AppDefinition *app = app_definition(app_for_route(ui_route()));
    const char *name = app ? tr(app->name) : "";
    int name_width = textwidth_raw(name,&small);
    if (name_width > 195) name_width = 195;
    if (name_width > right-left-12) name_width = right-left-12;
    if (name_width > 0) {
        int reference_height, reference_top=ink_top("Hg",&small,&reference_height);
        int name_height, name_top=ink_top(name,&small,&name_height);
        int baseline_y=(STATUS_HEIGHT-reference_height)/2-reference_top;
        label_ellipsis(left,baseline_y+name_top-(STATUS_HEIGHT-name_height)/2,
                       name_width,STATUS_HEIGHT,name,&small,0);
    }
    left += name_width+12;
    status_ui.status_timer_x = -1;
    extra_timer_label(timer, sizeof(timer));
    char alarm[16];
    extra_alarm_label(alarm, sizeof(alarm));
    if (*timer || *alarm) {
        if (*timer) {
            int m = 0, sec = 0;
            sscanf(timer, "%d:%d", &m, &sec);
            snprintf(timer, sizeof(timer), "%d min", m + (sec > 0));
        }
        char label[64];
        snprintf(label, sizeof(label), "%s%s%s", timer, *timer && *alarm ? " · " : "", alarm);
        int w = textwidth_raw(label, &caption) + 27;
        if (right - left > w + 6) {
            int x = left + (right - left - w) / 2;
            ring(x + 9, 28, 8, 2);
            stroke(x + 9, 28, x + 9, 23, 2);
            stroke(x + 9, 28, x + 13, 30, 2);
            centered(x + 19, 0, w - 19 + 16, STATUS_HEIGHT, label, &caption);
            status_ui.status_timer_x = left;
            status_ui.status_timer_width = right - left;
            system_region(left, 0, right - left, STATUS_HEIGHT);
        }
    }
}
