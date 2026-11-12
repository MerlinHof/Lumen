/* Book catalog, cover and Reader services use persistent book IDs. */
static int package_book_index(const char *key) {
    if(strlen(key)!=16 || strspn(key,"0123456789abcdef")!=16)return -1;
    uint64_t id=strtoull(key,NULL,16);
    for(int i=0;i<library_count;i++)if(library[i].id==id)return i;
    return -1;
}
static void package_book_field(FILE *out,int row,const char *name,const char *value) {
    char key[32];snprintf(key,sizeof(key),"%s%d",name,row);package_field(out,key,value);
}
static void package_book_summary(FILE *out,int row,const Book *book) {
    char key[20],author[256];
    snprintf(key,sizeof(key),"%016llx",(unsigned long long)book->id);
    package_book_field(out,row,"id",key);
    package_book_field(out,row,"title",book->title);
    package_book_field(out,row,"author",author_label(book->author,author,sizeof(author)));
    fprintf(out,",\"pinned%d\":%d,\"progress%d\":%d",row,book->favorite,row,
        book->total>0 ? (int)(100LL*(book->page+1)/book->total) : 0);
}
static int package_library(const char *operation,const char *data,int action) {
    if(!strcmp(operation,"sort")) {
        if(strlen(data)!=1 || data[0]<'0' || data[0]>'5')return package_acknowledge(action,0);
        settings.sort=data[0]-'0';library_settings_save();update_view();
        return package_acknowledge(action,1);
    }
    if(!strcmp(operation,"shelf")) {
        char *end;long requested=strtol(data,&end,10);
        if(end==data || *end!='\n' || requested<0 || requested>BOOK_LIMIT ||
           strlen(end+1)>=sizeof(library_ui.query))return package_acknowledge(action,0);
        if(library_ui.library_dirty)refresh_library();
        snprintf(library_ui.query,sizeof(library_ui.query),"%s",end+1);
        library_ui.listpage=(int)requested;update_view();
        char *message=NULL;size_t length=0;
        FILE *out=package_reply(&message,&length,action,1);
        if(!out)return 0;
        fprintf(out,",\"page\":%d,\"pages\":%d,\"sort\":%d",library_ui.listpage,library_pages(),settings.sort);
        int last=featured(),begin=library_begin();
        if(last>=0 && !library_ui.listpage)package_book_summary(out,6,&library[last]);
        for(int i=0;i<library_per() && begin+i<library_ui.view_count;i++)
            package_book_summary(out,i,&library[library_ui.view[begin+i]]);
        package_reply_end(out,&message);
        return 1;
    }
    const char *text=strchr(data,'\n');
    char key[17];
    if(text && text-data!=16)return package_acknowledge(action,0);
    snprintf(key,sizeof(key),"%.*s",text ? 16 : (int)sizeof(key),data);
    if(!text && strlen(data)!=16)return package_acknowledge(action,0);
    int index=package_book_index(key);
    if(index<0)return package_acknowledge(action,0);
    Book *book=&library[index];
    if(!strcmp(operation,"read")) {
        if(library_supported(book))return app_read_book(index) || package_acknowledge(action,0);
        ui_save();
        if(!amazon_reader_request(book->path))return package_acknowledge(action,0);
        package_exit_requested=1;
        return 1;
    }
    if(!strcmp(operation,"pin")) {
        book->favorite=!book->favorite;library_save_book(book);update_view();
        return package_acknowledge(action,1);
    }
    if(!strcmp(operation,"delete")) {
        if(current==index){ui_save();engine_close();current=-1;}
        int ok=library_delete(index);
        if(ok){selected=-1;update_view();}
        return package_acknowledge(action,ok);
    }
    if(text && text[1]) {
        text++;
        if(!strcmp(operation,"rename")) {
            if(current==index){ui_save();engine_close();current=-1;}
            return package_acknowledge(action,library_rename(index,text));
        }
        if(!strcmp(operation,"title"))return package_acknowledge(action,library_metadata(index,text,book->author));
        if(!strcmp(operation,"author"))return package_acknowledge(action,library_metadata(index,book->title,text));
    }
    if(strcmp(operation,"detail"))return package_acknowledge(action,0);
    char *message=NULL;size_t length=0;
    FILE *out=package_reply(&message,&length,action,1);
    if(!out)return 0;
    package_book_summary(out,0,book);
    package_field(out,"raw_author",book->author);
    const char *leaf=strrchr(book->path,'/');
    package_field(out,"filename",leaf ? leaf+1 : book->path);
    package_field(out,"format",book->format);
    fprintf(out,",\"supported\":%d,\"page\":%d,\"total\":%d,\"kilobytes\":%llu",
        library_supported(book),book->page,book->total,(unsigned long long)(book->size/1024));
    package_reply_end(out,&message);
    return 1;
}
