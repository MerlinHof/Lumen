#ifndef LUMEN_NOTES_COLLECTION_FORMAT_H
#define LUMEN_NOTES_COLLECTION_FORMAT_H
typedef struct {
    int id, parent;
    char name[96];
} NoteFolder;
typedef struct {
    int id, folder;
} NotePlace;
typedef struct {
    NoteFolder folder[512];
    NotePlace place[1024];
    int nf, np, current;
} NoteTree;
#endif
