/* Bounded snapshots of reading activity for packages. Book internals stay in the catalog. */
typedef struct {
    const Book *book;
    const BookInsight *insight;
} ReadingRow;
static int reading_compare(const void *a,const void *b) {
    const ReadingRow *left=a,*right=b;
    unsigned l=left->insight->seconds,r=right->insight->seconds;
    if(l!=r)return l>r ? -1 : 1;
    return left->book->id<right->book->id ? -1 : left->book->id!=right->book->id;
}
static int package_reading_activity(const char *operation,const char *data,int action) {
    if(!strcmp(operation,"month")) {
        char *end;long month=strtol(data,&end,10);
        if(!*data || *end || month<190001 || month>210012 || month%100<1 || month%100>12)return 0;
        char *message=NULL;size_t length=0;
        FILE *out=package_reply(&message,&length,action,1);
        if(!out)return 0;
        fprintf(out,",\"month\":%ld",month);
        for(int i=1;i<=31;i++) {
            ReadingDay *entry=app_day((int)month*100+i,0);
            fprintf(out,",\"day%d\":%u",i,entry ? entry->seconds : 0);
        }
        package_reply_end(out,&message);
        return 1;
    }
    if(strcmp(operation,"summary") && strcmp(operation,"books"))return 0;
    char *message=NULL;size_t length=0;
    FILE *out=package_reply(&message,&length,action,1);
    if(!out)return 0;
    if(!strcmp(operation,"summary")) {
        time_t now=time(NULL);
        struct tm day;localtime_r(&now,&day);day.tm_hour=12;
        int remaining=-1;
        unsigned estimate=0;
        if(current>=0 && current<library_count) {
            remaining=engine_count()-page-1;
            if(remaining<0)remaining=0;
            BookInsight *record=insight_get(library[current].id,0);
            if(record && record->pages>=3)
                estimate=(unsigned)((uint64_t)record->seconds*(unsigned)remaining/record->pages/60);
        }
        fprintf(out,",\"total\":%u,\"sessions\":%u,\"goal\":%d,\"remaining\":%d,\"estimate\":%u",
                app_data.seconds,insights.sessions,reading_goal,remaining,estimate);
        for(int i=0;i<31;i++,day.tm_mday--) {
            ReadingDay *entry=app_day(app_date_key(mktime(&day)),0);
            fprintf(out,",\"day%d\":%u",i,entry ? entry->seconds : 0);
        }
    } else {
        char *end;
        long requested=strtol(data,&end,10);
        if(!*data || *end || requested<0 || requested>=BOOK_LIMIT)requested=0;
        ReadingRow rows[BOOK_LIMIT];int count=0;
        for(int i=0;i<library_count;i++) {
            BookInsight *record=insight_get(library[i].id,0);
            if(record && (record->seconds || record->pages))
                rows[count++]=(ReadingRow){&library[i],record};
        }
        qsort(rows,count,sizeof(*rows),reading_compare);
        int offset=requested*5<count ? (int)requested*5 : 0;
        fprintf(out,",\"count\":%d,\"page\":%d",count,offset/5);
        for(int i=0;i<5 && offset+i<count;i++) {
            ReadingRow row=rows[offset+i];
            char key[20];snprintf(key,sizeof(key),"title%d",i);
            package_field(out,key,row.book->title);
            fprintf(out,",\"seconds%d\":%u,\"pages%d\":%u,\"sessions%d\":%u,\"position%d\":%d,\"length%d\":%d",
                    i,row.insight->seconds,i,row.insight->pages,i,row.insight->sessions,
                    i,row.book->page+1,i,row.book->total);
        }
    }
    package_reply_end(out,&message);
    return 1;
}
