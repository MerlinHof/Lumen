/* App-owned module; assembled by apps/host/compose.h. */
/* Small title cache, invalidated when notes are scanned after save, upload or
 * app activation. Scrolling/feedback redraws never reread whole note bodies. */
static unsigned note_list_generation;
static struct {
    int id;
    unsigned generation;
    char title[256];
} note_titles[14];
static unsigned note_title_slot;
static const char *note_list_title(int id) {
    for (int i = 0; i < 14; i++)
        if (note_titles[i].id == id && note_titles[i].generation == note_list_generation)
            return *note_titles[i].title ? note_titles[i].title : tr("Notiz");
    unsigned slot = note_title_slot++ % 14;
    note_titles[slot].id = id;
    note_titles[slot].generation = note_list_generation;
    char path[2048];
    snprintf(path, sizeof(path), "%s/notes/%d.txt", data_root, id);
    FILE *f = fopen(path, "r");
    note_titles[slot].title[0] = 0;
    if (f) {
        if (!fgets(note_titles[slot].title, sizeof(note_titles[slot].title), f))
            note_titles[slot].title[0] = 0;
        fclose(f);
    }
    note_titles[slot].title[strcspn(note_titles[slot].title, "\r\n")] = 0;
    return *note_titles[slot].title ? note_titles[slot].title : tr("Notiz");
}
static int note_newest_first(const void *a, const void *b) {
    int x = *(const int *)a, y = *(const int *)b;
    return (y > x) - (y < x);
}
static void notes_scan(void) {
    note_list_generation++;
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
