static pid_t fetch_worker;
static int fetch_result=-1;
static int accept_result;

static void fetch_image(const char *url,uint64_t id) {
    if(strncmp(url,"https://upload.wikimedia.org/",29) || strpbrk(url,"\r\n\t "))return;
    char path[64],leaf[256];
    article_path(path,id,"image.jpg");
    int parent=lumen_storage_parent(path,1,leaf);
    if(parent<0)return;
    const char *temp=".image-download";
    unlinkat(parent,temp,0);
    int fd=openat(parent,temp,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);
    if(fd>=0) {
        if(lumen_http_download((LumenHttpRequest){.url=url,.timeout=8},fd,1u<<20) && !fsync(fd))
            renameat(parent,temp,parent,leaf);
        close(fd);
        unlinkat(parent,temp,0);
    }
    close(parent);
}

static uint64_t fetch_article(const char *query,int language) {
    char encoded[769],url[1500],image[2048];
    if(!lumen_url_encode(query,encoded,sizeof(encoded)))return 0;
    char *body=malloc(2097153);
    Article *article=calloc(1,sizeof(*article));
    uint64_t id=0;
    if(!body || !article)goto done;
    for(int attempt=0;attempt<2 && !id;attempt++) {
        snprintf(url,sizeof(url),
            "https://%s.wikipedia.org/w/api.php?action=query&%s=%s&redirects=1&gsrnamespace=0&gsrlimit=1"
            "&prop=extracts%%7Cpageimages&piprop=thumbnail&pithumbsize=500&pilicense=free"
            "&explaintext=1&exsectionformat=wiki&exlimit=1&format=json&formatversion=2",
            article_language(language),attempt ? "generator=search&gsrsearch" : "titles",encoded);
        if(!lumen_https_get(url,body,2097153))break;
        Json json;
        JsonToken tokens[1024];
        if(!json_parse(&json,body,tokens,1024))continue;
        int pages=json_get(&json,json_get(&json,0,"query"),"pages"),index=json_at(&json,pages,0);
        json_string(&json,json_get(&json,index,"title"),article->title,sizeof(article->title));
        json_string(&json,json_get(&json,index,"extract"),article->text,sizeof(article->text));
        if(!*article->title || !*article->text)continue;
        json_string(&json,json_get(&json,json_get(&json,index,"thumbnail"),"source"),image,sizeof(image));
        article->magic=0x574b3131;
        article->language=language;
        article->seen=time(NULL);
        uint64_t key=article_id(article);
        char path[64];
        article_path(path,key,"bin");
        if(!lumen_storage_write(path,article,sizeof(*article)))break;
        article_path(path,key,"deleted");
        if(lumen_storage_exists(path) && !lumen_storage_delete(path))break;
        id=key;
        fetch_image(image,id);
    }
done:
    free(article);
    free(body);
    return id;
}
static int start_article(const char *query) {
    if(fetch_worker>0 || !*query)return 0;
    int result[2];
    if(pipe(result))return 0;
    fetch_worker=fork();
    if(!fetch_worker) {
        close(result[0]);
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        /* Reserve standard descriptors before storage opens files. */
        int null=open("/dev/null",O_RDWR);
        if(null!=STDIN_FILENO)_exit(1);
        if(dup2(null,STDOUT_FILENO)<0)_exit(1);
        uint64_t id=fetch_article(query,lumen_language);
        ssize_t written=write(result[1],&id,sizeof(id));
        _exit(written==(ssize_t)sizeof(id) ? 0 : 1);
    }
    close(result[1]);
    if(fetch_worker<0){close(result[0]);return 0;}
    fetch_result=result[0];
    accept_result=1;
    return 1;
}
static int finish_article(uint64_t *id) {
    if(fetch_worker<=0)return 0;
    int status;
    pid_t result=waitpid(fetch_worker,&status,WNOHANG);
    if(!result || (result<0 && errno==EINTR))return 0;
    *id=0;
    if(result>0 && WIFEXITED(status) && !WEXITSTATUS(status) &&
       read(fetch_result,id,sizeof(*id))!=(ssize_t)sizeof(*id))*id=0;
    close(fetch_result);
    fetch_result=-1;
    fetch_worker=0;
    return 1;
}
