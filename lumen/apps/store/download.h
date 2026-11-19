static int epubbooks_grant(const char *url, char *out, size_t cap) {
    const char *prefix = "https://www.epubbooks.com/downloads/";
    const char *id = url + strlen(prefix);
    if (!*id || strspn(id, "0123456789") != strlen(id) || strlen(id) > 11)
        return 0;
    char response[2048],payload[80];
    snprintf(response,sizeof(response),"%s/grant-%ld.tmp",getenv("LUMEN_APP_DATA"),(long)getpid());
    snprintf(payload,sizeof(payload),"{\"id\":%s}",id);
    pid_t p = fork();
    if (p < 0)
        return 0;
    if (!p) {
        execlp("curl", "curl", "-q", "--fail", "--silent", "--max-time", "20", "--proto", "=https", "-X",
               "POST", "--data", payload, "--output", response,
               "https://www.epubbooks.com/downloads", NULL);
        _exit(127);
    }
    if(!wait_success(p)){unlink(response);return 0;}
    FILE *f = fopen(response, "rb");
    if (!f)
        return 0;
    char text[2048];
    size_t n = fread(text, 1, sizeof(text) - 1, f);
    fclose(f);
    unlink(response);
    text[n] = 0;
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
static char current_url[2048],temporary[4096],job_file[96],download_title[256];
static int discover_busy(void) {return job>0 || import_pending;}
static const char *discover_url(void) {return current_url;}
static void discover_cancel(void) {
    if(job>0) {
        kill(job,SIGTERM);
        while(waitpid(job,NULL,0)<0 && errno==EINTR){}
        job=0;
    }
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
    job=fork();
    if(job<0){job=0;return 0;}
    if(!job) {
        int null=open("/dev/null",O_RDWR);
        if(null<0 || dup2(null,STDIN_FILENO)<0 || dup2(null,STDOUT_FILENO)<0)_exit(1);
        if(null>STDOUT_FILENO)close(null);
        char granted[2048];
        const char *source=url,*prefix="https://www.epubbooks.com/downloads/";
        if(book && !strncmp(url,prefix,strlen(prefix))) {
            if(!epubbooks_grant(url,granted,sizeof(granted)))_exit(1);
            source=granted;
        }
        rlim_t cap=book ? 128u<<20 : 4u<<20;
        struct rlimit limit={cap,cap};
        setrlimit(RLIMIT_FSIZE,&limit);
        execlp("curl","curl","-q","--fail","--silent","--location","--max-redirs","5",
               "--connect-timeout","8","--max-time",book ? "180" : "25","--proto","=http,https",
               "--proto-redir","=http,https","--user-agent","LUMEN (personal Kindle reader)",
               "--output",temporary,"--",source,(char *)NULL);
        _exit(127);
    }
    return 1;
}
static int discover_complete(void) {
    if(job<=0)return 0;
    int status;
    pid_t done=waitpid(job,&status,WNOHANG);
    if(!done || (done<0 && errno==EINTR))return 0;
    job=0;
    if(done<0 || !WIFEXITED(status) || WEXITSTATUS(status)) {
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
