/* App-owned module; assembled by apps/host/compose.h. */
static int v1_folder_for(int id) {
    for (int i = 0; i < note_tree.np; i++)
        if (note_tree.place[i].id == id)
            return note_tree.place[i].folder;
    return 0;
}
static void v1_place(int id, int folder) {
    for (int i = 0; i < note_tree.np; i++)
        if (note_tree.place[i].id == id) {
            note_tree.place[i].folder = folder;
            v1_dirty = 1;
            return;
        }
    if (note_tree.np < 1024) {
        note_tree.place[note_tree.np++] = (NotePlace){id, folder};
        v1_dirty = 1;
    }
}
