#ifndef LUMEN_NOTES_COLLECTION_STORE_H
#define LUMEN_NOTES_COLLECTION_STORE_H
#include <dirent.h>
#include <limits.h>
#include <sys/file.h>
#include "collection_format.h"

typedef struct {
    uint32_t magic, size;
    NoteTree tree;
} NoteTreeFile;

/* The foreground editor and a short-lived service worker may overlap.
 * All collection changes use the same lock file, which is never renamed. */
static int collection_lock(const char *root) {
    char leaf[256];
    int dir=lumen_storage_parent_at(root,"notes-collection.lock",0,leaf);
    if (dir<0) return -1;
    int fd=openat(dir,leaf,O_RDWR|O_CREAT|O_NONBLOCK|O_NOFOLLOW|O_CLOEXEC,0600);
    close(dir);
    if (fd<0) return -1;
    struct stat st;
    if (fstat(fd,&st) || !S_ISREG(st.st_mode) || flock(fd,LOCK_EX)) {
        close(fd);
        return -1;
    }
    return fd;
}

static int collection_index(const NoteTree *tree, int id) {
    for (int i=0; i<tree->nf; i++)
        if (tree->folder[i].id==id) return i;
    return -1;
}

static int collection_folder(const NoteTree *tree, int id) {
    for (int i=0; i<tree->np; i++)
        if (tree->place[i].id==id) return tree->place[i].folder;
    return 0;
}

static void collection_unplace(NoteTree *tree, int id) {
    for (int i=tree->np-1; i>=0; i--)
        if (tree->place[i].id==id) {
            tree->np--;
            memmove(tree->place+i,tree->place+i+1,(size_t)(tree->np-i)*sizeof(NotePlace));
        }
}

static int collection_place(NoteTree *tree, int id, int folder) {
    collection_unplace(tree,id);
    if (!folder) return 1;
    if (tree->np>=1024) return 0;
    tree->place[tree->np++]=(NotePlace){id,folder};
    return 1;
}

static int collection_load(const char *root, NoteTreeFile *file) {
    char leaf[256];
    int dir=lumen_storage_parent_at(root,"note-tree-1.bin",0,leaf);
    if (dir<0) return 0;
    struct stat st;
    int missing=fstatat(dir,leaf,&st,AT_SYMLINK_NOFOLLOW)<0 && errno==ENOENT;
    close(dir);
    if (missing) {
        memset(file,0,sizeof(*file));
        file->magic=0x4c554d16;
        file->size=sizeof(file->tree);
        return 1;
    }
    size_t length;
    if (!lumen_storage_read_at(root,"note-tree-1.bin",file,sizeof(*file),&length) ||
        length!=sizeof(*file) || file->magic!=0x4c554d16 ||
        file->size!=sizeof(file->tree) || file->tree.nf<0 || file->tree.nf>512 ||
        file->tree.np<0 || file->tree.np>1024) return 0;
    file->tree.current=0;
    for (int i=0; i<file->tree.nf; i++) {
        NoteFolder *folder=&file->tree.folder[i];
        folder->name[sizeof(folder->name)-1]=0;
        if (folder->id==folder->parent) folder->parent=0;
    }
    return 1;
}

static int collection_save(const char *root, const NoteTreeFile *file) {
    return lumen_storage_write_at(root,"note-tree-1.bin",file,sizeof(*file));
}

/* This path stays compatible with 6.0 and its web and ink readers. On creation
 * commit the optional folder reference before the note becomes visible. */
static int collection_create_note(const char *root, int folder, const char *text) {
    char leaf[256], path[48];
    int dir=lumen_storage_parent_at(root,"notes/file",1,leaf);
    if (dir<0) return 0;
    time_t now=time(NULL);
    int id=now>0 && now<INT_MAX ? (int)now : 1;
    for (;;) {
        struct stat st;
        snprintf(leaf,sizeof(leaf),"%d.txt",id);
        if (fstatat(dir,leaf,&st,AT_SYMLINK_NOFOLLOW)<0) {
            if (errno!=ENOENT) id=0;
            break;
        }
        if (id==INT_MAX) {id=0;break;}
        id++;
    }
    close(dir);
    if (!id) return 0;
    if (folder) {
        NoteTreeFile file;
        if (!collection_load(root,&file) || collection_index(&file.tree,folder)<0 ||
            !collection_place(&file.tree,id,folder) || !collection_save(root,&file)) return 0;
    }
    snprintf(path,sizeof(path),"notes/%d.txt",id);
    return lumen_storage_write_at(root,path,text,strlen(text)) ? id : 0;
}
#endif
