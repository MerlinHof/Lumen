/* Edition index and compatibility with previously saved editions. */
static int press_dates[2048], press_dates_count, press_open_pending;
static void press_archive_scan(void) {
    v1_archive_scan();
    press_dates_count = 0;
    for (int i = 0; i < news_date_count; i++)
        press_dates[press_dates_count++] = news_dates[i];
    char p[2048];
    local_path(p, sizeof(p), "press");
    DIR *d = opendir(p);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d)) && press_dates_count < 2048) {
            int date;
            char tail;
            if (sscanf(e->d_name, "edition-%8d.bin%c", &date, &tail) == 1 &&
                strlen(e->d_name) == 20) {
                int dup = 0;
                for (int i = 0; i < press_dates_count; i++)
                    if (press_dates[i] == date)
                        dup = 1;
                if (!dup)
                    press_dates[press_dates_count++] = date;
            }
        }
        closedir(d);
    }
    for (int i = 0; i < press_dates_count; i++)
        for (int j = i + 1; j < press_dates_count; j++)
            if (press_dates[j] > press_dates[i]) {
                int t = press_dates[i];
                press_dates[i] = press_dates[j];
                press_dates[j] = t;
            }
}
static void press_legacy(int date) {
    memset(&press_list, 0, sizeof(press_list));
    press_list.magic = 0x50524c31;
    press_list.date = date;
    if (news_load(date))
        for (int i = 0; i < newspaper.count; i++) {
            NewsArticle *a = &newspaper.article[i];
            if (a->category == 5)
                continue;
            PressItem *b = &press_list.item[press_list.count++];
            snprintf(b->title, 256, "%s", a->title);
            snprintf(b->url, 2048, "%s", a->url);
            snprintf(b->summary, 1600, "%s", a->body);
            snprintf(b->publisher, 96, "%s", a->publisher);
            snprintf(b->category, 48, "%s", news_category(a->category));
            snprintf(b->published, 96, "%s", a->published);
        }
}
static time_t press_retry_at;
static int press_daily_enabled;
