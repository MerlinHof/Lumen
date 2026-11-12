/* Wikipedia provider, cached document and shared Reader boundary. */
static int wiki_remove_archive(int index) {
    int deleting_current = wiki_entries[index].id == wiki_key();
    discover_cancel();
    char p[2048], source[512];
    const char *langs[] = {"de", "en", "fr", "zh"};
    snprintf(source, sizeof(source), "https://%s.wikipedia.org/wiki/%s",
             langs[wiki_entries[index].language],
             wiki_entries[index].title);
    uint64_t document = press_id(source);
    snprintf(p, sizeof(p), "%s/reader-documents/%016llx.html", data_root,
             (unsigned long long)document);
    unlink(p);
    snprintf(p, sizeof(p), "%s/reader-documents/%016llx.pgm", data_root,
             (unsigned long long)document);
    unlink(p);
    wiki_cache_path(p, sizeof(p), wiki_entries[index].id, "bin");
    if(unlink(p) && errno!=ENOENT)return 0;
    wiki_cache_path(p, sizeof(p), wiki_entries[index].id, "deleted");
    if(!atomic_write(p, "1", 1))return 0;
    wiki_cache_path(p, sizeof(p), wiki_entries[index].id, "pgm");
    unlink(p);
    local_path(p, sizeof(p), "wiki-last.xml");
    unlink(p);
    if (deleting_current) {
        discover_restore_lookup("", "", "", "");
        wiki_query[0] = 0;
        extra_write_text("wiki-query.txt", "");
        local_path(p, sizeof(p), "wiki-image.pgm");
        unlink(p);
        local_path(p, sizeof(p), "wiki-image-url.txt");
        unlink(p);
    }
    wiki_saved_hash = press_id(discover_lookup_text()) ^ wiki_key();
    wiki_delete_copies(wiki_entries[index].title);
    wiki_history_scan();
    return 1;
}
static int wiki_open_archive(int index) {
    char p[2048];
    wiki_cache_path(p, sizeof(p), wiki_entries[index].id, "bin");
    FILE *f = fopen(p, "rb");
    WikiSaved *a = malloc(sizeof(*a));
    int ok = f && a && fread(a, sizeof(*a), 1, f) == 1 && a->magic == 0x574b3131;
    if (f)
        fclose(f);
    if (ok) {
        a->title[255] = a->image[2047] = a->name[511] = a->text[524287] = 0;
        discover_cancel();
        wiki_ui.wiki_article_language = a->language >= 0 && a->language < 4 ? a->language : 0;
        discover_restore_lookup(a->title, a->text, a->image, a->name);
        wiki_saved_hash = press_id(a->text) ^ wiki_key();
        wiki_cache_path(p, sizeof(p), wiki_entries[index].id, "pgm");
        char target[2048];
        local_path(target, sizeof(target), "wiki-image.pgm");
        if (copy_file(p, target))
            extra_write_text("wiki-image-url.txt", a->image);
        wiki_hash = 0;
        char state[128];
        network_status(state, sizeof(state));
        if (device_wifi_enabled() && *state && strcmp(state, "Offline")) {
            snprintf(wiki_query, sizeof(wiki_query), "%s", a->title);
            wiki_refresh_id = wiki_entries[index].id;
            if (!discover_lookup(wiki_query, wiki_ui.wiki_article_language))
                ok=wiki_refresh_failed();
        } else
            ok=article_wiki_open();
    }
    free(a);
    return ok;
}
static int package_wikipedia(const char *operation,const char *data,int action) {
    if(!strcmp(operation,"search")) {
        int ok=0;
        if(*data && strlen(data)<sizeof(wiki_query)) {
            discover_cancel();
            wiki_refresh_id=0;
            wiki_ui.wiki_article_language=options[OPT_LANGUAGE];
            snprintf(wiki_query,sizeof(wiki_query),"%s",data);
            extra_write_text("wiki-query.txt",wiki_query);
            ok=discover_lookup(wiki_query,options[OPT_LANGUAGE]);
        }
        return package_acknowledge(action,ok);
    }
    if(wiki_history_dirty)wiki_history_scan();
    if(!strcmp(operation,"open") || !strcmp(operation,"remove")) {
        if(strlen(data)!=16 || strspn(data,"0123456789abcdef")!=16)return package_acknowledge(action,0);
        uint64_t id=strtoull(data,NULL,16);
        for(int i=0;i<wiki_entry_count;i++)if(wiki_entries[i].id==id) {
            if(!strcmp(operation,"open")) {
                if(wiki_open_archive(i))return 1;
                return package_acknowledge(action,0);
            }
            return package_acknowledge(action,wiki_remove_archive(i));
        }
        return package_acknowledge(action,0);
    }
    if(strcmp(operation,"list"))return 0;
    char *end;long page=strtol(data,&end,10);
    if(!*data || *end || page<0 || page>300 || page*7>=wiki_entry_count)page=0;
    char *message=NULL;size_t length=0;
    FILE *out=package_reply(&message,&length,action,1);
    if(!out)return 0;
    fprintf(out,",\"page\":%ld,\"count\":%d,\"busy\":%d",page,wiki_entry_count,
        discover_busy() || wiki_reader_pending);
    for(int i=0;i<7 && page*7+i<wiki_entry_count;i++) {
        int index=(int)page*7+i;
        char field[20],value[64];
        snprintf(value,sizeof(value),"%016llx",(unsigned long long)wiki_entries[index].id);
        snprintf(field,sizeof(field),"id%d",i);package_field(out,field,value);
        snprintf(field,sizeof(field),"title%d",i);package_field(out,field,wiki_entries[index].title);
        time_t stamp=wiki_entries[index].seen;
        struct tm local;value[0]=0;
        if(localtime_r(&stamp,&local))strftime(value,sizeof(value),"%d.%m.%Y · %H:%M",&local);
        snprintf(field,sizeof(field),"date%d",i);package_field(out,field,value);
    }
    package_reply_end(out,&message);
    return 1;
}
