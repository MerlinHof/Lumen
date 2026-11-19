static int dates[2048],date_count;
static int has_edition(int date) {
    for(int i=0;i<date_count;i++)if(dates[i]==date)return 1;
    return 0;
}
static void scan_editions(const char *root,const char *subdir,int modern) {
    if(!root || !*root)return;
    char path[4096];
    if(snprintf(path,sizeof(path),"%s/%s",root,subdir)>=(int)sizeof(path))return;
    DIR *dir=opendir(path);
    if(!dir)return;
    struct dirent *entry;
    while((entry=readdir(dir)) && date_count<2048) {
        const char *name=entry->d_name;
        if(modern) {
            if(strncmp(name,"edition-",8))continue;
            name+=8;
        }
        if(strlen(name)!=12 || strcmp(name+8,".bin") || strspn(name,"0123456789")!=8)continue;
        int date=atoi(name);
        if(date>20000101 && !has_edition(date))dates[date_count++]=date;
    }
    closedir(dir);
}
static int newest_first(const void *left,const void *right) {
    int a=*(const int *)left,b=*(const int *)right;
    return (b>a)-(b<a);
}
static void press_archive_scan(void) {
    date_count=1;
    dates[0]=news_date();
    const char *old=getenv("LUMEN_LEGACY_DATA");
    scan_editions(old,"news",0);
    scan_editions(old,"press",1);
    scan_editions(getenv("LUMEN_APP_DATA"),"press",1);
    qsort(dates,date_count,sizeof(dates[0]),newest_first);
}
static const char *news_category(int c) {
    const char *labels[]={"Welt","Wissenschaft","Technologie","Energie","An diesem Tag","Tagesrätsel"};
    return labels[c>=0 && c<6 ? c : 0];
}
static void press_legacy(int date) {
    memset(&press_list, 0, sizeof(press_list));
    press_list.magic = 0x50524c31;
    press_list.date = date;
    if (news_load(date))
        for (int i = 0; i < newspaper.count; i++) {
            NewsArticle *a = &newspaper.article[i];
            if (a->category == 5)
                continue;
            PressItem *b = &press_list.item[press_list.count++];
            snprintf(b->title, 256, "%s", a->title);
            snprintf(b->url, 2048, "%s", a->url);
            snprintf(b->summary, 1600, "%s", a->body);
            snprintf(b->publisher, 96, "%s", a->publisher);
            snprintf(b->category, 48, "%s", news_category(a->category));
            snprintf(b->published, 96, "%s", a->published);
        }
}

static void open_article(void) {
    if(!*press_article.body)return;
    char stem[48],relative[128],image[96];
    snprintf(stem,sizeof(stem),"%016llx",(unsigned long long)press_id(press_article.url));
    snprintf(relative,sizeof(relative),"press/%s.html",stem);
    snprintf(image,sizeof(image),"%s.pgm",stem);
    char image_path[128];
    snprintf(image_path,sizeof(image_path),"press/%s",image);
    int has_image=lumen_storage_exists(image_path);
    if(!has_image) {
        size_t length;unsigned char *bytes=malloc(540*280+4096);
        const char *legacy=getenv("LUMEN_LEGACY_DATA");
        has_image=bytes && legacy && lumen_storage_read_at(legacy,image_path,bytes,540*280+4096,&length) &&
                  lumen_storage_write(image_path,bytes,length);
        free(bytes);
    }
    char *html=NULL;size_t length=0;
    FILE *out=open_memstream(&html,&length);
    if(!out)return;
    fputs("<!doctype html><meta charset=\"utf-8\"><style>img{max-width:100%}p{margin:0 0 .65em}</style><title>",out);
    html_escape(out,press_article.title);
    fputs("</title><h1>",out);html_escape(out,press_article.title);
    fputs("</h1><p>",out);html_escape(out,press_article.publisher);fputs("</p>",out);
    if(has_image)fprintf(out,"<figure><img src=\"%s\"></figure>",image);
    for(const char *start=press_article.body;*start;) {
        size_t size=strcspn(start,"\n");
        if(size) {
            fputs("<p>",out);html_escape_n(out,start,size);fputs("</p>",out);
        }
        start+=size;
        if(*start)start++;
    }
    if(!strncmp(press_article.url,"https://",8) || !strncmp(press_article.url,"http://",7)) {
        fputs("<p><a href=\"",out);html_escape(out,press_article.url);
        fputs("\">",out);html_escape(out,press_article.title);fputs("</a></p>",out);
    }
    if(!fclose(out) && lumen_storage_write(relative,html,length))
        lumen_reader_document_title(relative,NULL,press_article.title);
    free(html);
}
static int daily_enabled(void) {
    if(lumen_storage_exists("daily-enabled"))return 1;
    const char *root=getenv("LUMEN_LEGACY_DATA");
    if(!root)return 0;
    char text[81]={0};size_t size;
    if(lumen_storage_read_at(root,"press-daily-enabled.txt",text,80,&size))return atoi(text)!=0;
    unsigned topics;int enabled;
    return lumen_storage_read_at(root,"news-settings.txt",text,80,&size) &&
           sscanf(text,"%u %d",&topics,&enabled)==2 && enabled;
}
static void daily_service(const char *operation,const char *data) {
    (void)data;
    if(strcmp(operation,"daily") || !daily_enabled()){lumen_app_service_result(0,"");return;}
    int date=news_date(),result=press_start_day(date,lumen_language);
    while(result==1) {
        usleep(200000);
        int done=press_poll();
        if(done){result=done>0 ? 2 : 0;break;}
    }
    lumen_app_service_result(result==2,"");
}
