/* Read the existing collection shared with the editor and export. */
#include <dirent.h>
#include <limits.h>
#include "collection_format.h"

static NoteTree collection;
static int note_ids_local[1024];

static int folder_index(int id) {
    for(int i=0;i<collection.nf;i++)
        if(collection.folder[i].id==id)return i;
    return -1;
}

static int note_folder(int id) {
    for(int i=0;i<collection.np;i++)
        if(collection.place[i].id==id)return collection.place[i].folder;
    return 0;
}

static int newest_first(const void *a,const void *b) {
    int left=*(const int *)a,right=*(const int *)b;
    return (right>left)-(right<left);
}

static int read_note_ids(const char *root) {
    char path[2048];
    if(!root || snprintf(path,sizeof(path),"%s/notes",root)>=(int)sizeof(path))return 0;
    int fd=open(path,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if(fd<0)return 0;
    DIR *dir=fdopendir(fd);
    if(!dir){close(fd);return 0;}
    int n=0;
    struct dirent *entry;
    while(n<1024 && (entry=readdir(dir))) {
        if(entry->d_name[0]<'1' || entry->d_name[0]>'9')continue;
        char *end;
        long id=strtol(entry->d_name,&end,10);
        struct stat info;
        if(id<=INT_MAX && !strcmp(end,".txt") &&
           !fstatat(fd,entry->d_name,&info,AT_SYMLINK_NOFOLLOW) && S_ISREG(info.st_mode))
            note_ids_local[n++]=(int)id;
    }
    closedir(dir);
    qsort(note_ids_local,(size_t)n,sizeof(note_ids_local[0]),newest_first);
    return n;
}

static void read_note_title(const char *root,int id,char *title,size_t capacity) {
    char name[48],leaf[256];
    snprintf(name,sizeof(name),"notes/%d.txt",id);
    int parent_fd=lumen_storage_parent_at(root,name,0,leaf);
    if(parent_fd<0)return;
    int fd=openat(parent_fd,leaf,O_RDONLY|O_NONBLOCK|O_NOFOLLOW|O_CLOEXEC);
    close(parent_fd);
    if(fd<0)return;
    FILE *file=fdopen(fd,"r");
    if(!file){close(fd);return;}
    if(fgets(title,(int)capacity,file)) {
        title[strcspn(title,"\r\n")]=0;
        /* A bounded first line may end inside a UTF-8 character. */
        size_t len=strlen(title);
        if(len==capacity-1) {
            size_t last=len-1;
            while(last && ((unsigned char)title[last]&0xc0)==0x80)last--;
            if((unsigned char)title[last]>=0x80)title[last]=0;
        }
    }
    fclose(file);
}

static void collection_list(void) {
    struct {uint32_t magic,size;NoteTree tree;} saved;
    memset(&collection,0,sizeof(collection));
    if(lumen_legacy_state("note-tree-1.bin",&saved,sizeof(saved)) && saved.magic==0x4c554d16 &&
       saved.size==sizeof(saved.tree) && saved.tree.nf>=0 && saved.tree.nf<=512 &&
       saved.tree.np>=0 && saved.tree.np<=1024)collection=saved.tree;
    int index=folder_index(folder);
    if(index<0)folder=0;
    parent=index>=0 ? collection.folder[index].parent : 0;
    if(parent==folder)parent=0;
    snprintf(folder_title,sizeof(folder_title),"%.*s",95,index>=0 ? collection.folder[index].name : "");
    const char *root=getenv("LUMEN_LEGACY_DATA");
    int notes=read_note_ids(root);
    int folders[1024];
    for(int i=0;i<notes;i++)folders[i]=note_folder(note_ids_local[i]);
    struct {int id,is_folder;} entries[1537];
    count=0;
    int moving=view==MOVE,per=moving ? 8 : 7;
    if(moving){entries[count].id=0;entries[count++].is_folder=1;}
    for(int i=0;i<collection.nf;i++)if(moving || collection.folder[i].parent==folder) {
        entries[count].id=collection.folder[i].id;entries[count++].is_folder=1;
    }
    if(!moving)for(int i=0;i<notes;i++)if(folders[i]==folder) {
        entries[count].id=note_ids_local[i];entries[count++].is_folder=0;
    }
    if(page<0 || page>=(count+per-1)/per)page=count ? (count-1)/per : 0;
    memset(rows,0,sizeof(rows));
    for(int i=0;i<per && page*per+i<count;i++) {
        int pos=page*per+i,id=entries[pos].id;
        rows[i].id=id;rows[i].kind=entries[pos].is_folder;
        if(rows[i].kind) {
            int at=folder_index(id);
            snprintf(rows[i].title,sizeof(rows[i].title),"%.*s",95,at>=0 ? collection.folder[at].name : word("Notizen","Notes","Notes"));
            for(int j=0;j<collection.nf;j++)rows[i].children+=collection.folder[j].parent==id;
            for(int j=0;j<notes;j++)rows[i].children+=folders[j]==id;
        } else {
            read_note_title(root,id,rows[i].title,sizeof(rows[i].title));
            if(!*rows[i].title)snprintf(rows[i].title,sizeof(rows[i].title),"%s",word("Notiz","Note","Note"));
            time_t stamp=id;struct tm day;
            if(localtime_r(&stamp,&day))strftime(rows[i].detail,sizeof(rows[i].detail),"%d.%m.%Y",&day);
        }
    }
}
