typedef struct {
    int book, index;
    char text[2048];
    ReaderPosition position;
} GlobalQuote;
static int quote_counts[BOOK_LIMIT], global_quote_count;

/* Reader annotation queries shared with independently packaged clients. */
static void quotes_load(void) {
    global_quote_count = 0;
    memset(quote_counts, 0, sizeof(quote_counts));
    for (int b = 0; b < library_count; b++) {
        char path[2048];
        snprintf(path, sizeof(path), "%s/books/%016llx.notes-v1", data_root,
                 (unsigned long long)library[b].id);
        struct stat st;
        if (!stat(path, &st) && st.st_size > 0) {
            int count = (int)(st.st_size / sizeof(Annotation));
            if (count > 128)
                count = 128;
            quote_counts[b] = count;
            global_quote_count += count;
        }
    }
}
static int quote_read(int b,int index,GlobalQuote *q) {
    if(b<0 || b>=library_count || index<0 || index>=128)return 0;
    char path[2048];
    snprintf(path, sizeof(path), "%s/books/%016llx.notes-v1", data_root,
             (unsigned long long)library[b].id);
    FILE *f = fopen(path, "rb");
    if (!f)
        return 0;
    Annotation a;
    int ok = !fseek(f, (long)index * sizeof(a), SEEK_SET) && fread(&a, sizeof(a), 1, f) == 1;
    fclose(f);
    if (!ok)
        return 0;
    a.text[sizeof(a.text) - 1] = 0;
    q->book = b;
    q->index = index;
    q->position = a.position;
    snprintf(q->text, sizeof(q->text), "%s", a.text);
    return 1;
}
static int quote_get(int index,GlobalQuote *q) {
    if(index<0)return 0;
    for(int b=0;b<library_count;b++) {
        if(index<quote_counts[b])return quote_read(b,index,q);
        index-=quote_counts[b];
    }
    return 0;
}
static int annotation_service(const char *operation,const char *data,int action) {
    if(strcmp(operation,"list") && strcmp(operation,"open"))return 0;
    if(!strcmp(operation,"open")) {
        if(strlen(data)<18 || data[16]!=':' || strspn(data,"0123456789abcdef")!=16)return 0;
        char *end;
        long index=strtol(data+17,&end,10);
        if(*end || index<0 || index>=128)return 0;
        BookId id=strtoull(data,NULL,16);
        int b=0;
        while(b<library_count && library[b].id!=id)b++;
        GlobalQuote quote;
        if(!quote_read(b,(int)index,&quote))return 0;
        AppIntent intent={.app=APP_READER,.kind=INTENT_BOOK,.object_id=id,
                         .has_position=1,.position=quote.position};
        return app_send(&intent);
    }
    char *end;
    long number=strtol(data,&end,10);
    if(!*data || *end || number<0 || number>128L*BOOK_LIMIT)return 0;
    quotes_load();
    int page=(int)number;
    if(page*4>=global_quote_count)page=0;
    char *message=NULL;size_t length=0;
    FILE *out=package_reply(&message,&length,action,1);
    if(!out)return 0;
    fprintf(out,",\"count\":%d,\"page\":%d",global_quote_count,page);
    for(int i=0;i<4;i++) {
        GlobalQuote quote;
        if(!quote_get(page*4+i,&quote))break;
        char key[16];
        snprintf(key,sizeof(key),"quote%d",i);package_field(out,key,quote.text);
        snprintf(key,sizeof(key),"book%d",i);package_field(out,key,library[quote.book].title);
        char ref[32];
        snprintf(ref,sizeof(ref),"%016llx:%d",(unsigned long long)library[quote.book].id,quote.index);
        snprintf(key,sizeof(key),"ref%d",i);package_field(out,key,ref);
    }
    package_reply_end(out,&message);
    return 1;
}
