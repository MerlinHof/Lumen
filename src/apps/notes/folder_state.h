/* App-owned state; stable on-disk schemas are intentionally preserved. */
typedef struct {
    int id, parent;
    char name[96];
} NoteFolder;
typedef struct {
    int id, folder;
} NotePlace;
static struct {
    NoteFolder folder[512];
    NotePlace place[1024];
    int nf, np, current;
} note_tree;
static int folder_page;
