/* App-owned module; assembled by apps/host/compose.h. */
static int journal_date17, journal_dates17[2048], journal_count17, journal_page17;
typedef struct {
    int version, date, score;
    char answers[6][8192];
} Journal17;
static void journal_store17(void) {
    if (!journal_date17)
        return;
    Journal17 *d = calloc(1, sizeof(*d));
    if (!d)
        return;
    d->version = 1;
    d->date = journal_date17;
    d->score = journal_score16;
    memcpy(d->answers, journal_answers16, sizeof(d->answers));
    char dir[2048], p[4096];
    local_path(dir, sizeof(dir), "journals17");
    mkdir(dir, 0700);
    snprintf(p, sizeof(p), "%s/%08d.bin", dir, journal_date17);
    char name[80];
    snprintf(name, 80, "journals17/%08d.bin", journal_date17);
    app_state_save(name, d, sizeof(*d));
    free(d);
}
static int journal_legacy17(int date) {
    for (int i = 0; i < note_count; i++) {
        char p[80];
        snprintf(p, 80, "notes/%d.txt", note_ids[i]);
        char *s = service_read(p, 65536);
        if (!s)
            continue;
        int day, month, year;
        int match = sscanf(s, "# Journal · %d.%d.%d", &day, &month, &year) == 3 &&
                    year * 10000 + month * 100 + day == date;
        if (match) {
            memset(journal_answers16, 0, sizeof(journal_answers16));
            char *scan = s;
            for (int k = 0; k < 6; k++) {
                char *h = strstr(scan, "\n## ");
                if (!h)
                    break;
                char *begin = strchr(h + 4, '\n');
                if (!begin)
                    break;
                begin++;
                while (*begin == '\n')
                    begin++;
                char *end = strstr(begin, "\n## ");
                if (!end)
                    end = strstr(begin, "\nStimmung:");
                if (!end)
                    end = begin + strlen(begin);
                int n = end - begin;
                while (n && begin[n - 1] == '\n')
                    n--;
                snprintf(journal_answers16[k], 8192, "%.*s", n > 8191 ? 8191 : n, begin);
                scan = end;
            }
            char *score = strstr(s, "Stimmung:");
            journal_score16 = score ? atoi(score + 9) : 3;
            free(s);
            return 1;
        }
        free(s);
    }
    return 0;
}
static void journal_open17(int date) {
    journal_date17 = date;
    journal_step16 = 0;
    journal_score16 = 3;
    memset(journal_answers16, 0, sizeof(journal_answers16));
    char name[80];
    snprintf(name, 80, "journals17/%08d.bin", date);
    Journal17 *d = malloc(sizeof(*d));
    if (d && app_state_load(name, d, sizeof(*d)) && d->version == 1 && d->date == date) {
        memcpy(journal_answers16, d->answers, sizeof(d->answers));
        journal_score16 = d->score;
        for (int i = 0; i < 6; i++)
            journal_answers16[i][8191] = 0;
    } else
        journal_legacy17(date);
    free(d);
    if (journal_score16 < 1 || journal_score16 > 5)
        journal_score16 = 3;
    extra_open(126);
}
static void journal_list17(void) {
    journal_count17 = 1;
    journal_dates17[0] = app_date_key(time(NULL));
    char p[2048];
    local_path(p, sizeof(p), "journals17");
    DIR *d = opendir(p);
    struct dirent *e;
    while (d && (e = readdir(d)) && journal_count17 < 2048) {
        int date, n = 0;
        if (sscanf(e->d_name, "%8d.bin%n", &date, &n) != 1 || n != 12 || e->d_name[n] ||
            date == journal_dates17[0])
            continue;
        journal_dates17[journal_count17++] = date;
    }
    if (d)
        closedir(d);
    for (int i = 0; i < note_count && journal_count17 < 2048; i++) {
        char name[80];
        snprintf(name, 80, "notes/%d.txt", note_ids[i]);
        char *s = service_read(name, 65536);
        if (!s)
            continue;
        int day, m, y;
        if (sscanf(s, "# Journal · %d.%d.%d", &day, &m, &y) == 3) {
            int date = y * 10000 + m * 100 + day, exists = 0;
            for (int k = 0; k < journal_count17; k++)
                if (journal_dates17[k] == date)
                    exists = 1;
            if (!exists)
                journal_dates17[journal_count17++] = date;
        }
        free(s);
    }
    for (int i = 0; i < journal_count17; i++)
        for (int k = i + 1; k < journal_count17; k++)
            if (journal_dates17[k] > journal_dates17[i]) {
                int n = journal_dates17[i];
                journal_dates17[i] = journal_dates17[k];
                journal_dates17[k] = n;
            }
}
