/* LUMEN 1.6: small app surfaces share persistence, reader and background jobs. */
static int u16_ready, auto_sleep_minutes = 10;
static int64_t activity16;

static const char *sleep_choices16[] = {"5 min", "10 min", "15 min", "30 min", "60 min", "Nie"};
static const int sleep_minutes16[] = {5, 10, 15, 30, 60, 0};
static char context_names16[258][64];
static const char *context_labels16[258];
static int context_count16;
static void u16_load(void) {
    if (u16_ready)
        return;
    u16_ready = 1;
    activity16 = extra_millis();
    char *s = service_read("auto-sleep16.txt", 20);
    if (s) {
        int n = atoi(s);
        for (int i = 0; i < 6; i++)
            if (n == sleep_minutes16[i])
                auto_sleep_minutes = n;
        free(s);
    }
    load_blob("task-categories16.bin", task_categories16, sizeof(task_categories16));
    for (int i = 0; i < 256; i++)
        task_categories16[i][63] = 0;
    s = service_read("calc-precision16.txt", 20);
    if (s) {
        int n = atoi(s);
        if (n >= 3 && n <= 15)
            calc_precision16 = n;
        free(s);
    }
}
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
static int task_matches16(int i) {
    return !*task_context16 || !strcmp(task_context16, task_categories16[i]);
}
static void contexts16(void) {
    context_count16 = 1;
    snprintf(context_names16[0], 64, "Alle Kontexte");
    for (int i = 0; i < task_count && context_count16 < 257; i++)
        if (*task_categories16[i]) {
            int found = 0;
            for (int j = 1; j < context_count16; j++)
                if (!strcmp(context_names16[j], task_categories16[i]))
                    found = 1;
            if (!found)
                snprintf(context_names16[context_count16++], 64, "%s", task_categories16[i]);
        }
    for (int i = 0; i < context_count16; i++)
        context_labels16[i] = context_names16[i];
}
static void u16_dropdown(int target, int choice) {
    if (target == 920) {
        u16_calendar_choice(choice);
    }
    if (target == 916) {
        reader_progress_mode = choice;
        char b[16];
        snprintf(b, 16, "%d", choice);
        extra_write_text("reader-progress15.txt", b);
    }
    if (target == 917 && choice >= 0 && choice < 6) {
        auto_sleep_minutes = sleep_minutes16[choice];
        char b[16];
        snprintf(b, 16, "%d", auto_sleep_minutes);
        extra_write_text("auto-sleep16.txt", b);
    }
    if (target == 918 && choice >= 0 && choice < context_count16) {
        snprintf(task_context16, 64, "%s", choice ? context_names16[choice] : "");
        task_page = 0;
    }
    if (target == 919) {
        calc_precision16 = choice == 0 ? 6 : choice == 1 ? 10 : 15;
        char b[20];
        snprintf(b, 20, "%d", calc_precision16);
        extra_write_text("calc-precision16.txt", b);
    }
}
static int u16_commit(int target, const char *text) {
    if (u17_commit(target, text))
        return 1;
    if (u16_apps_commit(target, text))
        return 1;
    if (target == 170) {
        if (category_task16 >= 0 && category_task16 < task_count) {
            snprintf(task_categories16[category_task16], 64, "%s", text);
            save_blob("task-categories16.bin", task_categories16, sizeof(task_categories16));
        }
        return 1;
    }
    return 0;
}
static int u16_draw(void) {
    u16_load();

    return u16_apps_draw();
}
static int u16_tick(void) {
    u16_load();
    if (!sleeping)
        device_activity();
    if (ink_dirty && !ink_drag && extra_millis() >= ink_save_at)
        ink_save();
    if (!sleeping && auto_sleep_minutes > 0 && !connect_running() &&
        extra_millis() - activity16 >= (int64_t)auto_sleep_minutes * 60000) {
        Input in = {.kind = INPUT_SLEEP};
        activity16 = extra_millis();
        device_request_sleep();
        return ui_input(&in);
    }
    return u16_jobs_tick();
}
static int u16_input(const Input *in) {
    u16_load();
    if (in->kind == INPUT_DOWN || in->kind == INPUT_TAP || in->kind == INPUT_SWIPE ||
        in->kind == INPUT_WAKE)
        activity16 = extra_millis();
    if (sleeping || keyboard || dropdown)
        return -1;
    if (in->kind == INPUT_TAP) {
        int x = in->x, y = in->y;

        if (screen == 9 && setting_category == 3 && y >= 590 && y < 642) {
            u17_sleep_select(590);
            return 1;
        }

        if (screen == 74 && y >= 74 && y < 118 && x >= 175 && x < 450) {
            contexts16();
            int selected = 0;
            for (int i = 1; i < context_count16; i++)
                if (!strcmp(context_names16[i], task_context16))
                    selected = i;
            open_dropdown(918, context_labels16, context_count16, selected, 175, 123, 275);
            return 1;
        }
    }
    return u16_apps_input(in);
}
