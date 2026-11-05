/* App-owned module; assembled by apps/host/compose.h. */
static const char *v1_folder_name(int id) {
    for (int i = 0; i < note_tree.nf; i++)
        if (note_tree.folder[i].id == id)
            return note_tree.folder[i].name;
    return tr("Notizen");
}

static void v1_notes_draw(void) {
    polish_notes();
}
static int v1_notes_input(const Input *in) {
    return polish_notes_input(in);
}
static void v1_note_tools(void) {
    u15_note_tools();
}
static void v1_folder_picker_draw(void) {
    heading("Notiz verschieben");
    int n = note_tree.nf + 1;
    for (int r = 0; r < 8 && folder_page * 8 + r < n; r++) {
        int i = folder_page * 8 + r, y = 145 + r * 62;
        region(28, y, 544, 58);
        icon(28, y + 4, IC_CONTENTS);
        drawtext_raw(88, y + 12, i ? note_tree.folder[i - 1].name : tr("Notizen"), ui_font(UI_PRIMARY), 480);
        if (i) {
            const char *p = v1_folder_name(note_tree.folder[i - 1].parent);
            drawtext_raw(88, y + 40, p, ui_font(UI_SECONDARY), 480);
        }
    }
    pager(699, &folder_page, (n + 7) / 8);
}
static int markdown_pages = 1;
