/* App-owned module; assembled by apps/host/compose.h. */
static void u17_choice(int target, int choice) {
    if (target == 936 && choice >= 0 && choice < folder_count17) {
        int dest = folder_ids17[choice];
        if (notes_kind17) {
            if (folder15_inside(dest, notes_id17)) {
                ui_message("Ziel liegt im selben Ordner.");
                return;
            }
            int f = folder15_index(notes_id17);
            if (f >= 0)
                note_tree.folder[f].parent = dest;
            v1_dirty = 1;
        } else
            v1_place(notes_id17, dest);
        v1_save();
        return;
    }

    if (target == 931) {
        calc_degrees = choice == 0;
        extra_save();
    }
    if (target == 932) {
        if (!choice)
            folder_menu17();
        else if (notes_kind17)
            u15_notes_action(2, notes_id17);
        else {
            note_load(notes_id17);
            extra_open(43);
        }
    }
    if (target == 933) {
        AppIntent i = {.app = APP_PLANETARIUM, .kind = INTENT_OPEN};
        if (!choice)
            app_send(&i);
        else
            extra_open(125);
    }
    u17_more_choice(target, choice);
}
static int u17_commit(int target, const char *text) {
    if (target == 180) {
        if (task_edit17 >= 0 && task_edit17 < task_count && *text) {
            snprintf(tasks[task_edit17].title, 200, "%s", text);
            v1_dirty = 1;
            v1_save();
        }
        return 1;
    }
    return u17_more_commit(target, text);
}
static int u17_input(const Input *in) {
    if (sleeping || keyboard || dropdown)
        return -1;
    if (in->kind == INPUT_TAP && screen == 40 && extra_page == 1 && in->y >= 304 && in->y < 364 &&
        in->x >= 300) {
        u17_sleep_select(304);
        return 1;
    }
    if (in->kind == INPUT_TAP && (screen == 79 || screen == 125) && in->y >= 74 && in->y < 118 &&
        in->x >= 300 && in->x < 516) {
        static const char *t[] = {"Sternhimmel", "Sonnensystem"};
        open_dropdown(933, t, 2, screen == 125, 300, 123, 216);
        return 1;
    }
    return u17_more_input(in);
}
static int u17_draw(void) {
    return u17_more_draw();
}
