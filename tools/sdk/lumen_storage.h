#ifndef LUMEN_STORAGE_H
#define LUMEN_STORAGE_H
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>

/* Resolve each parent through a directory descriptor. Symbolic links are rejected. */
static int lumen_storage_parent_at(const char *root,const char *name,int create,char leaf[256]) {
    if(!root || !*root || !name || !*name || strlen(name)>1024)return -1;
    int fd=open(root,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if(fd<0)return -1;
    for(const char *at=name;;) {
        const char *slash=strchr(at,'/');
        size_t length=slash ? (size_t)(slash-at) : strlen(at);
        if(!length || length>=256 || *at=='.' || memchr(at,'\\',length))break;
        memcpy(leaf,at,length);leaf[length]=0;
        if(!slash)return fd;
        int next=openat(fd,leaf,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
        if(next<0 && create && errno==ENOENT) {
            if((!mkdirat(fd,leaf,0700) || errno==EEXIST) && !fsync(fd))
                next=openat(fd,leaf,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
        }
        if(next<0)break;
        close(fd);fd=next;at=slash+1;
    }
    close(fd);return -1;
}
static int lumen_storage_parent(const char *name,int create,char leaf[256]) {
    return lumen_storage_parent_at(getenv("LUMEN_APP_DATA"),name,create,leaf);
}
static int lumen_storage_write_at(const char *root,const char *name,const void *bytes,size_t size) {
    char leaf[256],temporary[64];
    int parent=lumen_storage_parent_at(root,name,1,leaf);
    if(parent<0)return 0;
    static unsigned serial;
    int fd=-1;
    for(int i=0;i<32;i++) {
        snprintf(temporary,sizeof(temporary),".lumen-%ld-%u",(long)getpid(),++serial);
        fd=openat(parent,temporary,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
        if(fd>=0 || errno!=EEXIST)break;
    }
    if(fd<0){close(parent);return 0;}
    FILE *file=fdopen(fd,"wb");
    int ok=0;
    if(file) {
        ok=fwrite(bytes,1,size,file)==size && !fflush(file) && !fsync(fd);
        if(fclose(file))ok=0;
    } else close(fd);
    if(ok)ok=!renameat(parent,temporary,parent,leaf);
    if(ok)ok=!fsync(parent);
    else unlinkat(parent,temporary,0);
    close(parent);return ok;
}
static int lumen_storage_write(const char *name,const void *bytes,size_t size) {
    return lumen_storage_write_at(getenv("LUMEN_APP_DATA"),name,bytes,size);
}
static int lumen_storage_read_at(const char *root,const char *name,void *bytes,size_t capacity,size_t *actual) {
    char leaf[256];if(actual)*actual=0;
    int parent=lumen_storage_parent_at(root,name,0,leaf);
    if(parent<0)return 0;
    int fd=openat(parent,leaf,O_RDONLY|O_NONBLOCK|O_NOFOLLOW|O_CLOEXEC);
    close(parent);
    if(fd<0)return 0;
    struct stat file_status;
    if(fstat(fd,&file_status) || !S_ISREG(file_status.st_mode)){close(fd);return 0;}
    FILE *file=fdopen(fd,"rb");
    if(!file){close(fd);return 0;}
    size_t count=fread(bytes,1,capacity,file);
    int ok=fgetc(file)==EOF && !ferror(file);
    fclose(file);
    if(ok && actual)*actual=count;
    return ok;
}
static int lumen_storage_read(const char *name,void *bytes,size_t capacity,size_t *actual) {
    return lumen_storage_read_at(getenv("LUMEN_APP_DATA"),name,bytes,capacity,actual);
}
/* Directories and entries stay inside the current package's data root. */
static inline int lumen_storage_exists(const char *name) {
    char leaf[256];
    int parent=lumen_storage_parent(name,0,leaf);
    if(parent<0)return 0;
    struct stat status;
    int found=!fstatat(parent,leaf,&status,AT_SYMLINK_NOFOLLOW) &&
        (S_ISREG(status.st_mode) || S_ISDIR(status.st_mode));
    close(parent);
    return found;
}
static inline int lumen_storage_mkdir(const char *name) {
    char leaf[256];
    int parent=lumen_storage_parent(name,1,leaf);
    if(parent<0)return 0;
    int created=!mkdirat(parent,leaf,0700);
    int ok=created && !fsync(parent);
    if(!created && errno==EEXIST) {
        struct stat status;
        ok=!fstatat(parent,leaf,&status,AT_SYMLINK_NOFOLLOW) && S_ISDIR(status.st_mode);
    }
    close(parent);
    return ok;
}
/* Visit each regular file or directory. A nonzero callback result stops the
 * traversal successfully. Names are borrowed until the callback returns. */
static inline int lumen_storage_list(const char *name,
        int (*visit)(const char *name,int directory,void *context),void *context) {
    if(!name || !visit)return 0;
    int fd;
    if(!*name) {
        const char *root=getenv("LUMEN_APP_DATA");
        fd=root ? open(root,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC) : -1;
    } else {
        char leaf[256];
        int parent=lumen_storage_parent(name,0,leaf);
        if(parent<0)return 0;
        fd=openat(parent,leaf,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
        close(parent);
    }
    if(fd<0)return 0;
    DIR *directory=fdopendir(fd);
    if(!directory){close(fd);return 0;}
    int ok=1;
    for(;;) {
        errno=0;
        struct dirent *entry=readdir(directory);
        if(!entry){ok=!errno;break;}
        if(entry->d_name[0]=='.' || strchr(entry->d_name,'\\'))continue;
        struct stat status;
        if(fstatat(fd,entry->d_name,&status,AT_SYMLINK_NOFOLLOW)) {
            if(errno==ENOENT)continue;
            ok=0;break;
        }
        int is_dir=S_ISDIR(status.st_mode);
        if((is_dir || S_ISREG(status.st_mode)) && visit(entry->d_name,is_dir,context))break;
    }
    closedir(directory);
    return ok;
}
static int lumen_storage_delete(const char *name) {
    char leaf[256];
    int parent=lumen_storage_parent(name,0,leaf);
    if(parent<0)return 0;
    int ok=!unlinkat(parent,leaf,0) && !fsync(parent);
    close(parent);return ok;
}
#endif
