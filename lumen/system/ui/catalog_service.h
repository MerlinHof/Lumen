/* Catalog operations accept handles issued by the current provider. */
static int package_store(const char *operation,const char *data,int action) {
    if(!strcmp(operation,"search")) {
        if(strlen(data)>=sizeof(store_ui.store_query))return package_acknowledge(action,0);
        snprintf(store_ui.store_query,sizeof(store_ui.store_query),"%s",data);
        store_search();
        return package_acknowledge(action,1);
    }
    if(!strcmp(operation,"cancel")) {
        discover_cancel();
        store_ui.store_stage=0;
        return package_acknowledge(action,1);
    }
    if(!strcmp(operation,"parent")) {
        if(!store_ui.opds_depth)return package_acknowledge(action,0);
        discover_cancel();
        store_ui.store_stage=0;
        const char *parent=store_ui.opds_history[--store_ui.opds_depth];
        if(!strcmp(parent,"lumen:search"))store_restore();
        else if(!discover_start(parent,NULL))return package_acknowledge(action,0);
        return package_acknowledge(action,1);
    }
    if(!strcmp(operation,"browse") || !strcmp(operation,"download")) {
        int item=store_item(data),download=!strcmp(operation,"download");
        if(item<0 || !!opds_items[item].acquisition!=download ||
           (!download && store_ui.opds_depth>=16))return package_acknowledge(action,0);
        discover_cancel();
        store_ui.store_stage=0;
        if(!download)
            snprintf(store_ui.opds_history[store_ui.opds_depth++],2048,"%s",
                store_ui.store_federated ? "lumen:search" : discover_url());
        int ok=discover_start(opds_items[item].url,download ? &opds_items[item] : NULL);
        if(!download) {
            if(ok)store_ui.store_federated=0;
            else store_ui.opds_depth--;
        }
        return package_acknowledge(action,ok);
    }
    if(!strcmp(operation,"toggle_source") || !strcmp(operation,"add_source")) {
        u15_init();
        if(!strcmp(operation,"toggle_source")) {
            char *end;long index=strtol(data,&end,10);
            if(!*data || *end || index<0 || index>=source15_count)return package_acknowledge(action,0);
            source15[index].enabled=!source15[index].enabled;
        } else {
            if(source15_count>=10 || (strncmp(data,"https://",8) && strncmp(data,"http://",7)) ||
               strlen(data)>=2048 || strpbrk(data,"\r\n\"\\<> "))return package_acknowledge(action,0);
            int i=source15_count++;
            snprintf(source15[i].name,80,"Catalog %d",i-4);
            snprintf(source15[i].url,2048,"%s",data);
            source15[i].enabled=1;
        }
        sources15_save();
        return package_acknowledge(action,1);
    }
    int sources=!strcmp(operation,"sources"),featured=!strcmp(operation,"featured"),detail=!strcmp(operation,"detail");
    if(!sources && !featured && !detail && strcmp(operation,"list"))return 0;
    if(sources)u15_init();
    else store_seed();
    int count=sources ? source15_count : opds_count, per_page=sources ? 6 : featured ? 4 : 5;
    char *end;long page=strtol(data,&end,10);
    if(detail || !*data || *end || page<0 || page>100 || page*per_page>=count)page=0;
    int selected=detail ? store_item(data) : -1;
    if(detail && selected<0)return package_acknowledge(action,0);
    char *message=NULL;size_t length=0;
    FILE *out=package_reply(&message,&length,action,1);
    if(!out)return 0;
    fprintf(out,",\"count\":%d,\"page\":%ld,\"depth\":%d,\"busy\":%d",count,page,
        store_ui.opds_depth,discover_busy());
    if(detail)package_field(out,"summary",opds_items[selected].summary);
    unsigned seed=(unsigned)time(NULL)/86400;
    int first=count ? (int)(seed%(unsigned)count) : 0;
    for(int i=0;i<(detail ? 1 : per_page) && page*per_page+i<count;i++) {
        int index=detail ? selected : featured ? (first+i)%count : (int)page*per_page+i;
        char field[20],value[256],key[24];
        if(sources) {
            snprintf(key,sizeof(key),"%d",index);
            snprintf(field,sizeof(field),"title%d",i);package_field(out,field,source15[index].name);
            snprintf(field,sizeof(field),"author%d",i);package_field(out,field,source15[index].url);
            fprintf(out,",\"flag%d\":%d",i,source15[index].enabled);
        } else {
            OPDSItem *item=&opds_items[index];
            snprintf(key,sizeof(key),"%016llx",(unsigned long long)press_id(item->url));
            snprintf(field,sizeof(field),"title%d",i);package_field(out,field,item->title);
            snprintf(field,sizeof(field),"author%d",i);
            package_field(out,field,author_label(item->author,value,sizeof(value)));
            fprintf(out,",\"flag%d\":%d",i,!!item->acquisition);
        }
        snprintf(field,sizeof(field),"id%d",i);package_field(out,field,key);
    }
    package_reply_end(out,&message);
    return 1;
}
