/* App-owned module; assembled by apps/host/compose.h. */
static int note_newest_first(const void *a, const void *b) {
    int x = *(const int *)a, y = *(const int *)b;
    return (y > x) - (y < x);
}
static void notes_scan(void) {
    note_count = 0;
    char p[2048];
    local_path(p, sizeof(p), "notes");
    mkdir(p, 0700);
    DIR *d = opendir(p);
    if (!d)
        return;
    struct dirent *e;
    while ((e = readdir(d)) && note_count < 1024) {
        int id = 0, used = 0;
        if (sscanf(e->d_name, "%d.txt%n", &id, &used) == 1 && used > 0 && !e->d_name[used] &&
            id > 0) {
            char canonical[48];
            snprintf(canonical, sizeof(canonical), "%d.txt", id);
            if (!strcmp(canonical, e->d_name))
                note_ids[note_count++] = id;
        }
    }
    closedir(d);
    qsort(note_ids, note_count, sizeof(*note_ids), note_newest_first);
    if (note_page * 7 >= note_count)
        note_page = note_count ? (note_count - 1) / 7 : 0;
}
static void note_load(int id) {
    char name[80];
    snprintf(name, sizeof(name), "notes/%d.txt", id);
    note_text[0] = 0;
    extra_load_text(name, note_text, sizeof(note_text));
    note_selected = id;
}
