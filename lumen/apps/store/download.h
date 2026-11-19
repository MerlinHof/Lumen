static int epubbooks_grant(const char *url, char *out, size_t cap) {
    const char *prefix = "https://www.epubbooks.com/downloads/";
    const char *id = url + strlen(prefix);
    if (!*id || strspn(id, "0123456789") != strlen(id) || strlen(id) > 11)
        return 0;
    char text[2048],payload[80];
    snprintf(payload,sizeof(payload),"{\"id\":%s}",id);
    if(!lumen_http_text((LumenHttpRequest){.url="https://www.epubbooks.com/downloads",
        .json=payload,.timeout=20},text,sizeof(text)))return 0;
    char token[128];
    if(!wire_read(text) || !wire_bounded_string("id",token,sizeof(token)))return 0;
    if (!*token ||
        strspn(token, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_ ") !=
            strlen(token) ||
        strchr(token, ' '))
        return 0;
    snprintf(out, cap, "https://www.epubbooks.com/downloads/%s/file", token);
    return 1;
}

static pid_t job;
static int acquisition,import_pending;
static char current_url[2048],temporary[4096],partial[4104],job_file[96],download_title[256];
static int discover_busy(void) {return job>0 || import_pending;}
static const char *discover_url(void) {return current_url;}
static void discover_cancel(void) {
    lumen_worker_cancel(&job);
    if(*partial)unlink(partial);
    if(!import_pending && *job_file)lumen_storage_delete(job_file);
}
static int discover_start(const char *url,const OPDSItem *book) {
    if(discover_busy() || strlen(url)>=sizeof(current_url) ||
       (strncmp(url,"https://",8) && strncmp(url,"http://",7)))return 0;
    const char *root=getenv("LUMEN_APP_DATA");
    if(!root || !*root)return 0;
    acquisition=book!=NULL;
    if(book && strcmp(book->format,"pdf") && strcmp(book->format,"epub"))return 0;
    snprintf(current_url,sizeof(current_url),"%s",url);
    if(book)snprintf(download_title,sizeof(download_title),"%s",book->title);
    static unsigned serial;
    snprintf(job_file,sizeof(job_file),"store-%ld-%u.%s",(long)getpid(),++serial,book ? book->format : "xml");
    if(snprintf(temporary,sizeof(temporary),"%s/%s",root,job_file)>=(int)sizeof(temporary))return 0;
    snprintf(partial,sizeof(partial),"%s.part",temporary);
    job=lumen_worker_start();
    if(job<0){job=0;return 0;}
    if(!job) {
        char granted[2048];
        const char *source=url,*prefix="https://www.epubbooks.com/downloads/";
        if(book && !strncmp(url,prefix,strlen(prefix))) {
            if(!epubbooks_grant(url,granted,sizeof(granted)))_exit(1);
            source=granted;
        }
        int file=open(partial,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
        if(file<0)_exit(1);
        int ok=lumen_http_download((LumenHttpRequest){.url=source,.allow_http=1,.redirects=5,
            .connect_timeout=8,.timeout=book ? 180 : 25},file,book ? 128u<<20 : 4u<<20);
        if(close(file))ok=0;
        if(ok)ok=!rename(partial,temporary);
        if(!ok)unlink(partial);
        _exit(ok ? 0 : 1);
    }
    return 1;
}
static int discover_complete(void) {
    int result=lumen_worker_poll(&job);
    if(!result)return 0;
    if(result<0) {
        lumen_storage_delete(job_file);
        return -1;
    }
    if(acquisition) {
        import_pending=1;
        lumen_book_import(job_file,download_title,30);
        return 2;
    }
    int ok=discover_parse(temporary,current_url);
    lumen_storage_delete(job_file);
    return ok ? 1 : -1;
}
