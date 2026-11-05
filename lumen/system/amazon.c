#include "i18n.h"
#define _GNU_SOURCE
#include "amazon.h"
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <strings.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/resource.h>
static const char *tool(void) {
    const char *p = getenv("LUMEN_MOBITOOL");
    return p ? p : "/mnt/us/lumen/bin/mobitool";
}
int amazon_metadata(const char *path, char *title, size_t nt, char *author, size_t na,
                    int *encrypted) {
    int fds[2];
    if (pipe(fds))
        return 0;
    pid_t p = fork();
    if (!p) {
        close(fds[0]);
        dup2(fds[1], 1);
        dup2(fds[1], 2);
        close(fds[1]);
        execl(tool(), "mobitool", path, NULL);
        _exit(127);
    }
    close(fds[1]);
    FILE *f = fdopen(fds[0], "r");
    int found = 0;
    char line[2048];
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\r\n")] = 0;
            if (!strncmp(line, "Title: ", 7)) {
                snprintf(title, nt, "%s", line + 7);
                found = 1;
            } else if (!strncmp(line, "Author: ", 8))
                snprintf(author, na, "%s", line + 8);
            else if (strstr(line, "Document is encrypted"))
                *encrypted = 1;
        }
        fclose(f);
    } else
        close(fds[0]);
    if (p > 0)
        waitpid(p, NULL, 0);
    return found;
}
static char runtime_dir[256];
static void remove_owned_tree(const char *path,int depth) {
    if(depth>32)return;
    struct stat st;if(lstat(path,&st))return;
    if(!S_ISDIR(st.st_mode)){unlink(path);return;}
    DIR *dir=opendir(path);struct dirent *e;
    while(dir && (e=readdir(dir))){
        if(!strcmp(e->d_name,".")||!strcmp(e->d_name,".."))continue;
        char child[4096];if(snprintf(child,sizeof(child),"%s/%s",path,e->d_name)<(int)sizeof(child))remove_owned_tree(child,depth+1);
    }
    if(dir)closedir(dir);rmdir(path);
}
static void amazon_runtime_close(void) {
    if(*runtime_dir)remove_owned_tree(runtime_dir,0);
    runtime_dir[0]=0;
}
static int wait_child(pid_t pid) {
    int status=0;if(pid<=0)return 0;
    while(waitpid(pid,&status,0)<0){if(errno!=EINTR)return 0;}
    return WIFEXITED(status)&&WEXITSTATUS(status)==0;
}
static int read_serial(char *serial,size_t size) {
    const char *override=getenv("LUMEN_KINDLE_SERIAL");serial[0]=0;
    if(override)snprintf(serial,size,"%s",override);
    else {
        FILE *f=fopen("/proc/usid","r");
        if(f){if(!fgets(serial,(int)size,f))serial[0]=0;fclose(f);}
    }
    serial[strcspn(serial,"\r\n ")]=0;
    size_t n=strlen(serial);if(n!=16&&n!=40){serial[0]=0;return 0;}
    for(size_t i=0;i<n;i++)if(!isalnum((unsigned char)serial[i])){serial[0]=0;return 0;}
    return 1;
}
static int prepare_runtime(void) {
    if(*runtime_dir)return 1;
    char archive[2048];snprintf(archive,sizeof(archive),"%s/../lib/amazon-runtime.tar.gz",data_root);
    if(access(archive,R_OK))return 0;
    snprintf(runtime_dir,sizeof(runtime_dir),"/var/tmp/lumen-amazon-XXXXXX");
    if(!mkdtemp(runtime_dir)){runtime_dir[0]=0;return 0;}
    atexit(amazon_runtime_close);
    pid_t pid=fork();
    if(!pid){execl("/bin/tar","tar","-xzf",archive,"-C",runtime_dir,NULL);_exit(127);}
    if(!wait_child(pid)){amazon_runtime_close();return 0;}
    return 1;
}
int amazon_can_prepare(const Book *book) {
    if(!strcasecmp(book->format,"kfx")){
        char archive[2048];snprintf(archive,sizeof(archive),"%s/../lib/amazon-runtime.tar.gz",data_root);
        return !access(archive,R_OK) || getenv("LUMEN_KFX_CONVERTER");
    }
    return !strcasecmp(book->format,"azw") || !strcasecmp(book->format,"azw3") || !strcasecmp(book->format,"mobi");
}
static int copy_import(const char *from,const char *to) {
    char temp[2300];snprintf(temp,sizeof(temp),"%s.part-%ld",to,(long)getpid());
    FILE *in=fopen(from,"rb"),*out=NULL;int ok=0;
    if(in)out=fopen(temp,"wb");
    if(in&&out){unsigned char buffer[65536];size_t n;ok=1;
        while((n=fread(buffer,1,sizeof(buffer),in)))if(fwrite(buffer,1,n,out)!=n){ok=0;break;}
        if(ferror(in)||fflush(out)||fsync(fileno(out)))ok=0;
    }
    if(in)fclose(in);if(out && fclose(out))ok=0;
    if(ok)ok=rename(temp,to)==0;
    if(!ok)unlink(temp);return ok;
}
int amazon_prepare(const Book *book,char *path,size_t size) {
    if(library_import_path(book,path,size))return 1;
    if(!amazon_can_prepare(book))return 0;
    char parent[2048],work[2300],serial[64];
    snprintf(parent,sizeof(parent),"%s/imports",data_root);mkdir(parent,0700);
    snprintf(work,sizeof(work),"%s/.job-XXXXXX",parent);if(!mkdtemp(work))return 0;
    int kfx=!strcasecmp(book->format,"kfx");read_serial(serial,sizeof(serial));
    const char *converter=getenv("LUMEN_KFX_CONVERTER");
    if(kfx && !converter && (!*serial || !prepare_runtime())){remove_owned_tree(work,0);return 0;}
    char output[2600];snprintf(output,sizeof(output),"%s/book.epub",work);
    pid_t pid=fork();
    if(!pid){
        struct rlimit mem={192*1024*1024,192*1024*1024},cpu={180,180};setrlimit(RLIMIT_AS,&mem);setrlimit(RLIMIT_CPU,&cpu);
        int fd=open("/dev/null",O_WRONLY);if(fd>=0){dup2(fd,1);dup2(fd,2);close(fd);}
        if(kfx){
            if(converter){execl(converter,converter,book->path,output,NULL);_exit(127);}
            char loader[2048],python[2048],runner[2048],home[2048],libs[4096],pythonpath[4096],shim[2048];
            snprintf(home,sizeof(home),"%s/dist",runtime_dir);
            snprintf(loader,sizeof(loader),"%s/lib/runtime/ld-linux-armhf.so.3",home);
            snprintf(python,sizeof(python),"%s/bin/python3",home);snprintf(runner,sizeof(runner),"%s/import_kfx.py",home);
            snprintf(libs,sizeof(libs),"%s/lib/runtime:%s/lib/external",home,home);
            snprintf(pythonpath,sizeof(pythonpath),"%s:%s/lib/python3.11/site-packages",home,home);
            snprintf(shim,sizeof(shim),"%s/libsyscall_wrapper.so",runtime_dir);
            setenv("PYTHONHOME",home,1);setenv("PYTHONPATH",pythonpath,1);setenv("PYTHONDONTWRITEBYTECODE","1",1);
            setenv("PYTHONNOUSERSITE","1",1);setenv("LD_PRELOAD",shim,1);setenv("LD_LIBRARY_PATH",libs,1);setenv("LUMEN_KINDLE_SERIAL",serial,1);
            execl(loader,loader,"--library-path",libs,python,"-B",runner,book->path,output,NULL);
        }else if(*serial)execl(tool(),"mobitool","-P",serial,"-e","-o",work,book->path,NULL);
        else execl(tool(),"mobitool","-e","-o",work,book->path,NULL);
        _exit(127);
    }
    int ok=wait_child(pid);
    if(ok&&!kfx){DIR *d=opendir(work);struct dirent *e;ok=0;
        while(d&&(e=readdir(d))){size_t n=strlen(e->d_name);if(n>5&&!strcasecmp(e->d_name+n-5,".epub")){snprintf(output,sizeof(output),"%s/%s",work,e->d_name);ok=1;break;}}
        if(d)closedir(d);
    }
    snprintf(path,size,"%s/%016llx.epub",parent,(unsigned long long)book->id);
    if(ok){struct stat st;ok=!stat(output,&st)&&st.st_size>0&&copy_import(output,path);}
    if(ok){char stamp[2300],text[100];snprintf(stamp,sizeof(stamp),"%s/%016llx.source",parent,(unsigned long long)book->id);
        int n=snprintf(text,sizeof(text),"%llu %lld\n",(unsigned long long)book->size,(long long)book->modified);ok=atomic_write(stamp,text,n);
    }
    remove_owned_tree(work,0);return ok;
}

int amazon_reader_request(const char *path) {
    if (strncmp(path, "/mnt/us/documents/", 18) || strchr(path, '\n'))
        return 0;
    char uri[6000] = "app://com.lab126.booklet.reader";
    size_t n = strlen(uri);
    for (const unsigned char *p = (const unsigned char *)path; *p && n + 4 < sizeof(uri); p++) {
        unsigned char c = *p;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
            c == '/' || c == '-' || c == '_' || c == '.')
            uri[n++] = (char)c;
        else
            n += (size_t)snprintf(uri + n, sizeof(uri) - n, "%%%02X", c);
    }
    uri[n] = 0;
    char output[2048];
    snprintf(output, sizeof(output), "%s/../reader-request.txt", data_root);
    return atomic_write(output, uri, n);
}
