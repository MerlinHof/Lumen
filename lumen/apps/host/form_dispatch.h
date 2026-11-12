/* App-owned module; assembled by apps/host/compose.h. */
static void u17_choice(int target, int choice) {
    if (target == 936 && choice >= 0 && choice < folder_count17) {
        int dest = folder_ids17[choice];
        if (notes_ui.notes_kind17) {
            if (folder15_inside(dest, notes_ui.notes_id17)) {
                ui_message("Ziel liegt im selben Ordner.");
                return;
            }
            int f = folder15_index(notes_ui.notes_id17);
            if (f >= 0)
                note_tree.folder[f].parent = dest;
            v1_dirty = 1;
        } else
            v1_place(notes_ui.notes_id17, dest);
        v1_save();
        return;
    }

    if (target == 932) {
        if (!choice)
            folder_menu17();
        else if (notes_ui.notes_kind17)
            u15_notes_action(2, notes_ui.notes_id17);
        else {
            note_load(notes_ui.notes_id17);
            extra_open(43);
        }
    }
}
static int u17_commit(int target, const char *text) {
    (void)target; (void)text; return 0;
}
static int u17_input(const Input *in) {
    if (sleeping || editor_ui.keyboard || dropdown)
        return -1;
    return -1;
}
static int u17_draw(void) {
    return 0;
}
