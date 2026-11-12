/* Public edition/search operations. Only provider-issued article keys can be opened. */
static int package_news(const char *operation,const char *data,int action) {
    if(!strcmp(operation,"search")) {
        if(!*data || strlen(data)>=sizeof(press_list.query))return package_acknowledge(action,0);
        press_cancel();
        press_open_pending=0;
        memset(&press_list,0,sizeof(press_list));
        snprintf(press_list.query,sizeof(press_list.query),"%s",data);
        return package_acknowledge(action,press_search(data,options[OPT_LANGUAGE]));
    }
    if(!strcmp(operation,"edition")) {
        press_archive_scan();
        if(strlen(data)!=8 || strspn(data,"0123456789")!=8)return package_acknowledge(action,0);
        int date=atoi(data),found=0;
        for(int i=0;i<press_dates_count;i++)if(press_dates[i]==date){found=1;break;}
        if(!found)return package_acknowledge(action,0);
        press_cancel();
        press_open_pending=0;
        if(!press_load_day(date)) {
            press_legacy(date);
            if(date==news_date())press_start_day(date,options[OPT_LANGUAGE]);
        }
        return package_acknowledge(action,1);
    }
    if(!strcmp(operation,"open")) {
        if(strlen(data)!=16 || strspn(data,"0123456789abcdef")!=16)return package_acknowledge(action,0);
        uint64_t key=strtoull(data,NULL,16);
        for(int i=0;i<press_list.count;i++)if(press_id(press_list.item[i].url)==key) {
            press_cancel();
            press_open_pending=0;
            int result=press_open(&press_list.item[i]);
            if(result==2)return article_news_open() || package_acknowledge(action,0);
            if(result==1)press_open_pending=1;
            return package_acknowledge(action,result==1);
        }
        return package_acknowledge(action,0);
    }
    int editions=!strcmp(operation,"editions");
    if(!editions && strcmp(operation,"articles"))return 0;
    if(editions) {
        if(!press_daily_enabled) {
            press_daily_enabled=1;
            extra_write_text("press-daily-enabled.txt","1");
        }
        press_archive_scan();
    }
    int count=editions ? press_dates_count : press_list.count, per_page=editions ? 7 : 5;
    char *end;
    long page=strtol(data,&end,10);
    if(!*data || *end || page<0 || page>500 || page*per_page>=count)page=0;
    char *message=NULL;size_t length=0;
    FILE *out=package_reply(&message,&length,action,1);
    if(!out)return 0;
    fprintf(out,",\"page\":%ld,\"count\":%d,\"busy\":%d,\"today\":%d",
            page,count,press_busy(),news_date());
    if(!editions) {
        package_field(out,"query",press_list.query);
        fprintf(out,",\"date\":%u",press_list.date);
    }
    for(int i=0;i<per_page && page*per_page+i<count;i++) {
        int index=(int)page*per_page+i;
        char field[20],value[180];
        snprintf(field,sizeof(field),"id%d",i);
        if(editions) {
            snprintf(value,sizeof(value),"%08d",press_dates[index]);
            package_field(out,field,value);
        } else {
            PressItem *item=&press_list.item[index];
            snprintf(value,sizeof(value),"%016llx",(unsigned long long)press_id(item->url));
            package_field(out,field,value);
            snprintf(field,sizeof(field),"title%d",i);package_field(out,field,item->title);
            snprintf(value,sizeof(value),"%s · %s",tr(item->category),item->publisher);
            snprintf(field,sizeof(field),"detail%d",i);package_field(out,field,value);
        }
    }
    package_reply_end(out,&message);
    return 1;
}
