static void calendar_shift16(int step) {
    struct tm t = {.tm_year = calendar_year - 1900,
                   .tm_mon = calendar_month - 1,
                   .tm_mday = calendar_day,
                   .tm_hour = 12};
    if (calendar_view16 == 0)
        t.tm_mday += step;
    else if (calendar_view16 == 1)
        t.tm_mday += 7 * step;
    else if (calendar_view16 == 3)
        t.tm_year += step;
    else
        t.tm_mon += step;
    mktime(&t);
    calendar_year = t.tm_year + 1900;
    calendar_month = t.tm_mon + 1;
    calendar_day = t.tm_mday;
    app_calendar_load();
}
