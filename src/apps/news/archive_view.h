/* App-owned module; assembled by apps/host/compose.h. */
static void v1_archive_scan(void) {
    news_date_count = 0;
    news_dates[news_date_count++] = news_date();
    char p[2048];
    local_path(p, sizeof(p), "news");
    DIR *d = opendir(p);
    if (!d)
        return;
    struct dirent *e;
    while ((e = readdir(d)) && news_date_count < 2048) {
        int n;
        char tail;
        if (sscanf(e->d_name, "%8d.bin%c", &n, &tail) == 1 && strlen(e->d_name) == 12 &&
            n != news_date() && n > 20000101 && n <= 120261231)
            news_dates[news_date_count++] = n;
    }
    closedir(d);
    for (int i = 1; i < news_date_count; i++)
        for (int j = i + 1; j < news_date_count; j++)
            if (news_dates[j] > news_dates[i]) {
                int t = news_dates[i];
                news_dates[i] = news_dates[j];
                news_dates[j] = t;
            }
}
static void v1_archive_draw(void) {
    polish_news_draw();
}
