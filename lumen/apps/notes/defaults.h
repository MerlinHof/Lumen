/* App-owned module; assembled by apps/host/compose.h. */
static void u16_notes_defaults(void) {
    static int initialized;
    if (initialized)
        return;
    initialized = 1;
    /* No legacy drawing conversion. New folders are created only once. */
    char p[2048];
    local_path(p, sizeof(p), "notes-defaults16.txt");
    if (!access(p, F_OK))
        return;
    const char *names[] = {"Journals", "Notes", "Ideas", "Learning"};
    for (int n = 0; n < 4 && note_tree.nf < 512; n++) {
        int exists = 0, max = 0;
        for (int i = 0; i < note_tree.nf; i++) {
            if (note_tree.folder[i].id > max)
                max = note_tree.folder[i].id;
            if (!note_tree.folder[i].parent && !strcmp(note_tree.folder[i].name, names[n]))
                exists = 1;
        }
        if (!exists) {
            NoteFolder *f = &note_tree.folder[note_tree.nf++];
            f->id = max + 1;
            f->parent = 0;
            snprintf(f->name, 96, "%s", names[n]);
        }
    }
    v1_dirty = 1;
    v1_save();
    atomic_write(p, "1", 1);
}
static void note_delete16(int id) {
    if (ink_note == id) {
        ink_dirty = ink_drag = 0;
        ink_note = ink_page = -1;
        note_ink_mode = 0;
    }
    char dir[2048];
    local_path(dir, sizeof(dir), "notes");
    DIR *d = opendir(dir);
    struct dirent *e;
    while (d && (e = readdir(d))) {
        int found = 0, page = 0, n = 0;
        char name[96];
        snprintf(name, sizeof(name), "%d.txt", id);
        int match = !strcmp(name, e->d_name);
        if (sscanf(e->d_name, "%d-page-%d.ink%n", &found, &page, &n) == 2 && n > 0 &&
            !e->d_name[n] && found == id)
            match = 1;
        if (match) {
            char p[4096];
            snprintf(p, sizeof(p), "%s/%s", dir, e->d_name);
            unlink(p);
        }
    }
    if (d)
        closedir(d);
    for (int i = note_tree.np - 1; i >= 0; i--)
        if (note_tree.place[i].id == id) {
            memmove(note_tree.place + i, note_tree.place + i + 1,
                    (note_tree.np - i - 1) * sizeof(NotePlace));
            note_tree.np--;
        }
    if (note_selected == id) {
        note_selected = -1;
        note_text[0] = 0;
    }
    v1_dirty = 1;
    v1_save();
}
