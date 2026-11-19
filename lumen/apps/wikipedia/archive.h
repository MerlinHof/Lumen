/* Keep the legacy record layout so old archives can be read in place. */
typedef struct {
    uint32_t magic;
    int32_t language;
    int64_t seen;
    char title[256], image[2048], name[512], text[524288];
} Article;

typedef struct {
    uint64_t id;
    int64_t seen;
    char title[256];
    int legacy;
} ArchiveRow;

static ArchiveRow archive[2048];
static int archive_count;

static uint64_t article_id(const Article *article) {
    char key[280];
    snprintf(key,sizeof(key),"%d:%s",article->language,article->title);
    uint64_t hash=1469598103934665603ULL;
    for(const unsigned char *p=(const unsigned char *)key;*p;p++)
        hash=(hash^*p)*1099511628211ULL;
    return hash;
}
static void article_path(char path[64],uint64_t id,const char *extension) {
    snprintf(path,64,"wiki-archive/%016llx.%s",(unsigned long long)id,extension);
}
static FILE *open_archive_record(const char *root,const char *name) {
    char leaf[256];
    int parent=lumen_storage_parent_at(root,name,0,leaf);
    if(parent<0)return NULL;
    int fd=openat(parent,leaf,O_RDONLY|O_NOFOLLOW|O_NONBLOCK);
    close(parent);
    if(fd<0)return NULL;
    struct stat info;
    if(fstat(fd,&info) || !S_ISREG(info.st_mode)) { close(fd);return NULL; }
    FILE *file=fdopen(fd,"rb");
    if(!file)close(fd);
    return file;
}
static void scan_archive(const char *root,int legacy) {
    if(!root || !*root)return;
    char path[4096];
    if(snprintf(path,sizeof(path),"%s/wiki-archive",root)>=(int)sizeof(path))return;
    DIR *dir=opendir(path);
    if(!dir)return;
    struct dirent *item;
    while((item=readdir(dir)) && archive_count<2048) {
        if(strlen(item->d_name)!=20 || strspn(item->d_name,"0123456789abcdef")!=16 ||
           strcmp(item->d_name+16,".bin"))continue;
        uint64_t id=strtoull(item->d_name,NULL,16);
        char name[64];
        article_path(name,id,"deleted");
        if(lumen_storage_exists(name))continue;
        int i=0;
        while(i<archive_count && archive[i].id!=id)i++;
        if(i<archive_count)continue;
        article_path(name,id,"bin");
        FILE *file=open_archive_record(root,name);
        if(!file)continue;
        struct {uint32_t magic;int32_t language;int64_t seen;char title[256];} entry;
        int ok=fread(&entry,sizeof(entry),1,file)==1 && entry.magic==0x574b3131;
        fclose(file);
        if(!ok)continue;
        ArchiveRow *row=&archive[archive_count++];
        *row=(ArchiveRow){.id=id,.seen=entry.seen,.legacy=legacy};
        snprintf(row->title,sizeof(row->title),"%.*s",255,entry.title);
        article_path(name,id,"opened");
        file=open_archive_record(getenv("LUMEN_APP_DATA"),name);
        if(!file && legacy) {
            char folded[768];
            lumen_utf8_fold(row->title,folded,sizeof(folded));
            uint64_t hash=1469598103934665603ULL;
            for(const unsigned char *p=(const unsigned char *)folded;*p;p++)hash=(hash^*p)*1099511628211ULL;
            article_path(name,hash,"opened");
            file=open_archive_record(root,name);
        }
        if(file) {
            long long stamp;
            if(fscanf(file,"%lld",&stamp)==1 && stamp>0 && stamp<=INT64_MAX/2)row->seen=stamp/1000;
            fclose(file);
        }
    }
    closedir(dir);
}
static int newest_article(const void *left,const void *right) {
    const ArchiveRow *a=left,*b=right;
    if(a->seen!=b->seen)return a->seen>b->seen ? -1 : 1;
    return strcmp(a->title,b->title);
}
static void refresh_archive(void) {
    archive_count=0;
    scan_archive(getenv("LUMEN_APP_DATA"),0);
    scan_archive(getenv("LUMEN_LEGACY_DATA"),1);
    qsort(archive,archive_count,sizeof(archive[0]),newest_article);
}
static void html_escape(FILE *out,const char *text,size_t length) {
    for(size_t i=0;i<length;i++) {
        switch(text[i]) {
        case '&':fputs("&amp;",out);break;
        case '<':fputs("&lt;",out);break;
        case '>':fputs("&gt;",out);break;
        case '"':fputs("&quot;",out);break;
        default:fputc((unsigned char)text[i],out);break;
        }
    }
}
static const char *article_language(int index) {
    static const char *const languages[]={"de","en","fr","zh"};
    return languages[index>=0 && index<4 ? index : 0];
}
static int open_article(uint64_t id,int legacy) {
    const char *root=getenv(legacy ? "LUMEN_LEGACY_DATA" : "LUMEN_APP_DATA");
    char path[64];
    article_path(path,id,"bin");
    Article *article=calloc(1,sizeof(*article));
    if(!article)return 0;
    size_t length;
    int ok=lumen_storage_read_at(root,path,article,sizeof(*article),&length) &&
           length==sizeof(*article) && article->magic==0x574b3131;
    if(!ok){free(article);return 0;}
    article->text[sizeof(article->text)-1]=article->title[sizeof(article->title)-1]=0;
    char *html=NULL;
    FILE *out=open_memstream(&html,&length);
    if(!out){free(article);return 0;}
    fputs("<!doctype html><meta charset=utf-8><style>img{max-width:100%;height:auto}</style><h1>",out);
    html_escape(out,article->title,strlen(article->title));
    fputs("</h1><p>Wikipedia</p>",out);
    char image[64];
    article_path(image,id,legacy ? "pgm" : "image.jpg");
    if(legacy && !lumen_storage_exists(image)) {
        unsigned char *pixels=malloc(2097152);
        size_t size;
        if(pixels && lumen_storage_read_at(root,image,pixels,2097152,&size))
            lumen_storage_write(image,pixels,size);
        free(pixels);
    }
    if(lumen_storage_exists(image))fprintf(out,"<figure><img src=\"%s\"></figure>",strrchr(image,'/')+1);
    for(const char *p=article->text;*p;) {
        size_t n=strcspn(p,"\n"),level=0;
        while(level<n && p[level]=='=')level++;
        const char *tag=level>2 ? "h3" : level==2 ? "h2" : "p";
        size_t first=level>=2 ? level : 0,last=n;
        if(first)while(last>first && (p[last-1]=='=' || p[last-1]==' '))last--;
        if(last>first) {
            fprintf(out,"<%s>",tag);
            html_escape(out,p+first,last-first);
            fprintf(out,"</%s>\n",tag);
        }
        p+=n+(p[n]=='\n');
    }
    char encoded[770];
    lumen_url_encode(article->title,encoded,sizeof(encoded));
    fprintf(out,"<p><a href=\"https://%s.wikipedia.org/wiki/%s\">Wikipedia</a> · "
        "<a href=\"https://creativecommons.org/licenses/by-sa/4.0/\">CC BY-SA</a></p>",
        article_language(article->language),encoded);
    article_path(path,id,"html");
    ok=!fclose(out) && lumen_storage_write(path,html,length);
    if(ok) {
        char stamp[32],opened[64];
        struct timespec now;
        clock_gettime(CLOCK_REALTIME,&now);
        int n=snprintf(stamp,sizeof(stamp),"%lld",(long long)now.tv_sec*1000+now.tv_nsec/1000000);
        article_path(opened,id,"opened");
        lumen_storage_write(opened,stamp,(size_t)n);
        lumen_reader_document_title(path,NULL,article->title);
    }
    free(html);
    free(article);
    return ok;
}
static int remove_article(uint64_t id) {
    char path[64];
    article_path(path,id,"deleted");
    if(!lumen_storage_write(path,"1",1))return 0;
    static const char *const extensions[]={"bin","html","pgm","image.jpg","opened"};
    for(size_t i=0;i<sizeof(extensions)/sizeof(*extensions);i++) {
        article_path(path,id,extensions[i]);
        if(lumen_storage_exists(path))lumen_storage_delete(path);
    }
    return 1;
}
