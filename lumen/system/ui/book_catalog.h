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
/* Shared catalog query. Page geometry, featured cards and persisted sort
 * preferences belong to the caller. */
static int catalog_contains_line(const char *lines,const char *name) {
    for(const char *at=lines;*at;) {
        size_t size=strcspn(at,"\n");
        if(strlen(name)==size && !memcmp(name,at,size))return 1;
        at+=size+(at[size]=='\n');
    }
    return 0;
}
static int package_book_query(const char *data,int action) {
    int offset,limit,sort,flags,read=0;
    if(sscanf(data,"%d %d %d %d%n",&offset,&limit,&sort,&flags,&read)!=4 ||
       offset<0 || offset>BOOK_LIMIT || limit<1 || limit>16 || sort< -1 || sort>5 ||
       flags<0 || flags>3 || data[read]!='\n')return package_acknowledge(action,0);
    const char *query=data+read+1,*filters=strchr(query,'\n');
    if(!filters || filters-query>=128 || strlen(filters+1)>512)return package_acknowledge(action,0);
    char term[128];
    snprintf(term,sizeof(term),"%.*s",(int)(filters-query),query);
    if(library_dirty)refresh_library();
    int order[BOOK_LIMIT],visible[BOOK_LIMIT];
    if(sort<0)sort=settings.sort>=0 && settings.sort<=5 ? settings.sort : 0;
    int count=library_view(order,term,0,sort),found=0,recent=library_last();
    for(int pass=0;pass<((flags&1) ? 2 : 1);pass++) {
        for(int i=0;i<count;i++) {
            int index=order[i];
            const Book *book=&library[index];
            const char *name=strrchr(book->path,'/');
            name=name ? name+1 : book->path;
            if(((flags&1) && (!!book->favorite)!=(pass==0)) ||
               ((flags&2) && index==recent) || catalog_contains_line(filters+1,name))continue;
            visible[found++]=index;
        }
    }
    char *message=NULL;size_t length=0;
    FILE *out=package_reply(&message,&length,action,1);
    if(!out)return 0;
    fprintf(out,",\"count\":%d,\"sort\":%d,\"has_recent\":%d",found,sort,recent>=0);
    for(int i=0;i<limit && offset+i<found;i++)
        package_book_summary(out,i,&library[visible[offset+i]]);
    if(recent>=0)package_book_summary(out,16,&library[recent]);
    package_reply_end(out,&message);
    return 1;
}
static int package_books(const char *operation,const char *data,int action) {
    if(!strcmp(operation,"query"))return package_book_query(data,action);
    if(library_dirty)refresh_library();
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
        book->favorite=!book->favorite;library_save_book(book);
        return package_acknowledge(action,1);
    }
    if(!strcmp(operation,"delete")) {
        if(current==index){ui_save();engine_close();current=-1;}
        int ok=library_delete(index);
        if(ok)selected=-1;
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
