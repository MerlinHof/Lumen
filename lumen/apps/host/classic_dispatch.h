/* App-owned module; assembled by apps/host/compose.h. */
static void apps_draw(void) {
    switch (ui_route()) {
    case 21:
        app_calendar();
        break;
    case 22:
        app_insights();
        break;
    }
}
static int apps_input(const Input *in) {
    int x = in->x, y = in->y;
    if(in->kind!=INPUT_TAP)return 2;
    if (ui_route() == 21) {
        if (y >= 74 && y < 118 && x >= 468) {
            if (x >= 522) {
                static char date_title[80];
                snprintf(date_title, sizeof(date_title), "%02d.%02d.%04d", calendar_day,
                         calendar_month, calendar_year);
                open_keyboard(8, date_title, calendar_note);
            } else {
                time_t now = time(NULL);
                struct tm t;
                localtime_r(&now, &t);
                calendar_year = t.tm_year + 1900;
                calendar_month = t.tm_mon + 1;
                calendar_day = t.tm_mday;
                app_calendar_load();
            }
            return 1;
        }
        if (y >= 681 && y < 721 && x < 80) {
            extra_open(45);
            return 1;
        }
        if (y >= 154 && y < 198) {
            if (x >= 80 && x <= 520) {
                char b[32];
                snprintf(b, sizeof(b), "%04d-%02d", calendar_year, calendar_month);
                open_keyboard(40, "Monat: YYYY-MM", b);
                return 1;
            }
            if (x < 80) {
                calendar_month--;
                if (calendar_month < 1) {
                    calendar_month = 12;
                    calendar_year--;
                }
            } else if (x > 520) {
                calendar_month++;
                if (calendar_month > 12) {
                    calendar_month = 1;
                    calendar_year++;
                }
            }
            if (calendar_year < 1900)
                calendar_year = 1900;
            if (calendar_year > 2100)
                calendar_year = 2100;
            if (calendar_day > calendar_days(calendar_year, calendar_month))
                calendar_day = calendar_days(calendar_year, calendar_month);
            app_calendar_load();
        } else if (y >= 258 && y < 606 && x >= 27 && x < 573) {
            struct tm first = {.tm_year = calendar_year - 1900,
                               .tm_mon = calendar_month - 1,
                               .tm_mday = 1,
                               .tm_hour = 12};
            mktime(&first);
            int d = ((y - 258) / 58) * 7 + (x - 27) / 78 - (first.tm_wday + 6) % 7 + 1;
            if (d >= 1 && d <= calendar_days(calendar_year, calendar_month)) {
                calendar_day = d;
                app_calendar_load();
            }
        } else if (x >= 421 && y >= 622 && y < 666)
            open_keyboard(8, "Tagesnotiz", calendar_note);
        else if (x >= 421 && y >= 681 && y < 721) {
            time_t now = time(NULL);
            struct tm t;
            localtime_r(&now, &t);
            calendar_year = t.tm_year + 1900;
            calendar_month = t.tm_mon + 1;
            calendar_day = t.tm_mday;
            app_calendar_load();
        }
        return 1;
    }
    return 1;
}
