/* App-owned module; assembled by apps/host/compose.h. */
/* Compact system controls and app refinements, shared by every application. */
static void u17_gray_icon(int x, int y) {
    for (int yy = y; yy < y + 44 && yy < 800; yy++)
        for (int xx = x; xx < x + 44 && xx < 600; xx++)
            if (xx >= 0 && yy >= 0) {
                unsigned char *v = &ui_pixels[yy * 600 + xx];
                if (*v < 255)
                    *v = 135 + *v * 120 / 255;
            }
}
static int u17_notes_depth(void) {
    return note_tree.current != 0;
}
static void u17_notes_up(void) {
    int i = folder15_index(note_tree.current);
    note_tree.current = i >= 0 ? note_tree.folder[i].parent : 0;
    tree_page = 0;
}
static void u17_notes_menu(int kind, int id, int y) {
    notes_ui.notes_kind17 = kind;
    notes_ui.notes_id17 = id;
    static const char *actions[] = {"Verschieben", "Löschen"};
    open_dropdown(932, actions, 2, -1, 328, y + 46, 244);
}
