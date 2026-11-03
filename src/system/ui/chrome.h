/* System-owned status and navigation bars. No app content. */
static void tabbar(void) {
    rect(0, 736, 600, 64, 255);
    rect(28, 736, 544, 2, 190);
    int count = options[OPT_APPS] ? 4 : 3, width = 600 / count;
    int active = app_definition(app_for_route(screen))->tab;
    const int kinds[] = {IC_HOME, IC_SETTINGS, IC_STORE, IC_GRID};
    const char *labels[] = {"Home", "Einstellungen", "Store", "Apps"};
    for (int i = 0; i < count; i++) {
        int x = i * width;
        system_region(x, 737, width, 63);
        icon(x + width / 2 - 22, options[OPT_TAB_LABELS] ? 742 : 748, kinds[i]);
        if (options[OPT_TAB_LABELS])
            centered(x, 779, width, 20, labels[i], &caption);
        if (i == active)
            rect(x + width / 2 - 28, 736, 56, 4, 0);
    }
}
static void header(void) {
    time_t now = time(NULL);
    if (now - status_time > 30) {
        device_status(battery, sizeof(battery), wifi, sizeof(wifi));
        status_time = now;
        battery_charging = device_charging();
    }
    rect(0, 0, 600, 48, 255);
    int left = can_back() ? 68 : 24;
    if (can_back()) {
        system_region(0, 0, 62, 48);
        icon(8, 0, IC_BACK);
    }
    char date[32], clock[16], percent[24], timer[32];
    struct tm t;
    localtime_r(&now, &t);
    strftime(date, sizeof(date), "%d.%m.", &t);
    strftime(clock, sizeof(clock), "%H:%M", &t);
    snprintf(percent, sizeof(percent), "%d%%", atoi(battery));
    if (options[OPT_DATE]) {
        centered(left - 8, 0, textwidth(date, &small) + 16, 44, date, &small);
        left += textwidth(date, &small) + 12;
    }
    if (options[OPT_CLOCK]) {
        centered(left - 8, 0, textwidth(clock, &small) + 16, 44, clock, &small);
        left += textwidth(clock, &small) + 12;
    }
    int right = 576, previous = -1;
    status_wifi_x = status_battery_x = -1;
    for (int element = 0; element < 3; element++) {
        int enabled = element == 0   ? options[OPT_BATTERY]
                      : element == 1 ? options[OPT_BATTERY_PERCENT]
                      : element == 2 ? options[OPT_WIFI]
                                     : options[OPT_DATE];
        if (!enabled)
            continue;
        int gap = previous < 0 ? 0 : element == 1 ? 8 : 16;
        right -= gap;
        const char *label = element == 1 ? percent : date;
        int width = element == 0   ? (battery_charging ? 50 : 34)
                    : element == 2 ? 30
                                   : textwidth(label, &small),
            x = right - width;
        if (element == 0) {
            status_battery_x = x;
            if (battery_charging)
                x += 16;
            status_battery_right = right;
            aa_soft_rect(x, 15, 30, 18, 4, 0);
            aa_soft_rect(x + 2, 17, 26, 14, 2, 255);
            aa_soft_rect(x + 31, 21, 3, 6, 1, 0);
            int fill = 22 * atoi(battery) / 100;
            if (fill > 22)
                fill = 22;
            if (fill > 0)
                aa_soft_rect(x + 4, 19, fill, 10, 2, 0);
            if (battery_charging) {
                int bx = x - 13;
                stroke(bx + 8, 14, bx + 1, 25, 3);
                stroke(bx + 1, 25, bx + 9, 24, 3);
                stroke(bx + 9, 24, bx + 3, 34, 3);
                stroke(bx + 3, 34, bx + 6, 23, 2);
            }
            system_region(status_battery_x - 5, 0, battery_charging ? 60 : 44, 48);
        } else if (element == 2) {
            status_wifi_x = x;
            wifi_symbol(x, 2);
            if (!strcmp(wifi, "Offline"))
                aa_stroke(x + 2, 11, x + 27, 34, 2);
            system_region(x - 5, 0, 40, 48);
        } else {
            centered(x - 8, 0, width + 16, 44, label, &small);
            if (element == 1) {
                system_region(x - 4, 0, width + 8, 48);
                if (status_battery_x < 0 || x < status_battery_x) {
                    status_battery_x = x;
                    if (status_battery_right < right || !options[OPT_BATTERY])
                        status_battery_right = right;
                }
            }
        }
        right = element == 0 && battery_charging ? x - 16 : x;
        previous = element;
    }
    status_timer_x = -1;
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
            ring(x + 9, 22, 8, 2);
            stroke(x + 9, 22, x + 9, 17, 2);
            stroke(x + 9, 22, x + 13, 24, 2);
            centered(x + 19, 0, w - 19 + 16, 44, label, &caption);
            status_timer_x = left;
            status_timer_width = right - left;
            system_region(left, 0, right - left, 48);
        }
    }
}
