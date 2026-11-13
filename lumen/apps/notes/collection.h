/* Read the existing collection shared with the editor and export. */
#include "collection_store.h"

static NoteTreeFile collection_file;
#define collection (collection_file.tree)
static int note_ids_local[1024];

static int collection_new_folder(NoteTree *tree,int parent,const char *name) {
    if (!*name || strlen(name)>=sizeof(tree->folder[0].name) || tree->nf>=512) return 0;
    int id=1;
    while(collection_index(tree,id)>=0)id++;
    NoteFolder *folder=&tree->folder[tree->nf++];
    *folder=(NoteFolder){.id=id,.parent=parent};
    snprintf(folder->name,sizeof(folder->name),"%s",name);
    return id;
}

static int collection_initialize(const char *root, NoteTreeFile *file) {
    if (!collection_load(root,file)) return 0;
    char leaf[256];
    int dir=lumen_storage_parent_at(root,"notes-defaults16.txt",0,leaf);
    if (dir<0) return 0;
    struct stat st;
    int missing=fstatat(dir,leaf,&st,AT_SYMLINK_NOFOLLOW)<0 && errno==ENOENT;
    close(dir);
    if (!missing) return 1;
    const char *names[]={"Journals","Notes","Ideas","Learning"};
    NoteTree *tree=&file->tree;
    for (size_t n=0; n<sizeof(names)/sizeof(*names); n++) {
        int exists=0;
        for (int i=0; i<tree->nf; i++)
            if (!tree->folder[i].parent && !strcmp(tree->folder[i].name,names[n])) exists=1;
        if (!exists) collection_new_folder(tree,0,names[n]);
    }
    return collection_save(root,file) && lumen_storage_write_at(root,"notes-defaults16.txt","1",1);
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
    const char *root=getenv("LUMEN_LEGACY_DATA");
    int lock=collection_lock(root);
    if (lock<0 || !collection_initialize(root,&collection_file)) memset(&collection,0,sizeof(collection));
    if (lock>=0) close(lock);
    int index=collection_index(&collection,folder);
    if(index<0)folder=0;
    parent=index>=0 ? collection.folder[index].parent : 0;
    if(parent==folder)parent=0;
    snprintf(folder_title,sizeof(folder_title),"%.*s",95,index>=0 ? collection.folder[index].name : "");
    int notes=read_note_ids(root);
    int folders[1024];
    for(int i=0;i<notes;i++)folders[i]=collection_folder(&collection,note_ids_local[i]);
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
            int at=collection_index(&collection,id);
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

static int collection_contains(const NoteTree *tree,int folder,int ancestor) {
    for (int depth=0; folder && depth<tree->nf; depth++) {
        if (folder==ancestor) return 1;
        int index=collection_index(tree,folder);
        if (index<0) break;
        folder=tree->folder[index].parent;
    }
    return 0;
}

static int collection_remove_note(const char *root,int id) {
    char leaf[256], note[48], ink[48];
    int fd=lumen_storage_parent_at(root,"notes/file",0,leaf);
    if (fd<0) return errno==ENOENT;
    DIR *dir=fdopendir(fd);
    if (!dir) {close(fd);return 0;}
    snprintf(note,sizeof(note),"%d.txt",id);
    snprintf(ink,sizeof(ink),"%d-page-",id);
    size_t prefix=strlen(ink);
    int ok=1;
    for (;;) {
        errno=0;
        struct dirent *entry=readdir(dir);
        if (!entry) {if (errno) ok=0;break;}
        const char *name=entry->d_name;
        if (strncmp(name,ink,prefix)) continue;
        const char *suffix=name+prefix;
        size_t digits=strspn(suffix,"0123456789");
        if (digits && !strcmp(suffix+digits,".ink") && unlinkat(fd,name,0) && errno!=ENOENT)
            ok=0;
    }
    if (ok && unlinkat(fd,note,0) && errno!=ENOENT) ok=0;
    if (ok) ok=fsync(fd)==0;
    closedir(dir);
    return ok;
}

static int collection_edit_folder(const char *root,const char *name,int rename_folder) {
    if (!*name || strlen(name)>=96 || !collection_initialize(root,&collection_file)) return 0;
    int index=collection_index(&collection,folder);
    if (folder && index<0) return 0;
    if (rename_folder) {
        if (index<0) return 0;
        snprintf(collection.folder[index].name,sizeof(collection.folder[index].name),"%s",name);
    } else if (!collection_new_folder(&collection,folder,name)) return 0;
    return collection_save(root,&collection_file);
}

static int collection_move(const char *root,int kind,int id,int destination) {
    if (!collection_initialize(root,&collection_file) ||
        (destination && collection_index(&collection,destination)<0)) return 0;
    if (kind) {
        int index=collection_index(&collection,id);
        if (index<0 || collection_contains(&collection,destination,id)) return 0;
        collection.folder[index].parent=destination;
    } else {
        char path[48],leaf[256];
        snprintf(path,sizeof(path),"notes/%d.txt",id);
        int dir=lumen_storage_parent_at(root,path,0,leaf);
        if (dir<0) return 0;
        struct stat st;
        int exists=!fstatat(dir,leaf,&st,AT_SYMLINK_NOFOLLOW) && S_ISREG(st.st_mode);
        close(dir);
        if (!exists || !collection_place(&collection,id,destination)) return 0;
    }
    return collection_save(root,&collection_file);
}

static int collection_delete(const char *root,int kind,int id) {
    if (!collection_initialize(root,&collection_file)) return 0;
    if (!kind) {
        if (!collection_remove_note(root,id)) return 0;
        collection_unplace(&collection,id);
    } else {
        if (collection_index(&collection,id)<0) return 0;
        int remove[512];
        for (int i=0; i<collection.nf; i++)
            remove[i]=collection_contains(&collection,collection.folder[i].id,id);
        for (int i=collection.np-1; i>=0; i--) {
            NotePlace place=collection.place[i];
            if (!collection_contains(&collection,place.folder,id)) continue;
            if (!collection_remove_note(root,place.id)) return 0;
            collection_unplace(&collection,place.id);
        }
        for (int i=collection.nf-1; i>=0; i--)
            if (remove[i]) {
                collection.nf--;
                memmove(collection.folder+i,collection.folder+i+1,(size_t)(collection.nf-i)*sizeof(NoteFolder));
            }
    }
    return collection_save(root,&collection_file);
}

static int create_learning(const char *root,const char *data) {
    const char *split=strchr(data,'\n');
    if (!split || split-data>255 || !split[1] || !collection_initialize(root,&collection_file)) return 0;
    int destination=0;
    for (int i=0; i<collection.nf; i++)
        if (!strcmp(collection.folder[i].name,"Learning")) destination=collection.folder[i].id;
    char title[256], text[8192];
    snprintf(title,sizeof(title),"%.*s",(int)(split-data),data);
    int length=snprintf(text,sizeof(text),word("# Abrufübung: %s\n\n%s","# Recall practice: %s\n\n%s","# Rappel actif : %s\n\n%s"),
                        *title ? title : word("Lernen","Study","Étude"),split+1);
    return length>0 && (size_t)length<sizeof(text) ? collection_create_note(root,destination,text) : 0;
}

static void serve(const char *operation,const char *data) {
    if (strcmp(operation,"create_learning")) {lumen_app_service_result(0,"");return;}
    const char *root=getenv("LUMEN_LEGACY_DATA");
    int lock=collection_lock(root);
    int id=lock>=0 ? create_learning(root,data) : 0;
    if (lock>=0) close(lock);
    char result[24]="";
    if (id) snprintf(result,sizeof(result),"%d",id);
    lumen_app_service_result(id!=0,result);
}
