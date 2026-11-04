/* App-owned module; assembled by apps/host/compose.h. */
static int news_today_cached(void) {
    if (newspaper.date == (unsigned)news_date())
        return 1;
    char n[80], p[2048];
    snprintf(n, 80, "news/%08d.bin", news_date());
    local_path(p, sizeof(p), n);
    return access(p, R_OK) == 0;
}
