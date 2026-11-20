/* Session-scoped document engine. The Reader app owns navigation and controls. */
static int reader_value(int id) {
    switch(id) {
    case 0:return options[OPT_FONT];
    case 1:return settings.size;
    case 2:return settings.leading;
    case 3:return settings.margin;
    case 4:return options[OPT_ALIGN];
    case 5:return options[OPT_PARAGRAPH];
    case 6:return options[OPT_LETTERSPACE];
    case 7:return options[OPT_SWIPE];
    case 8:return options[OPT_TAP];
    case 9:return options[OPT_PROGRESS] ? 0 : 5;
    default:return 0;
    }
}
static int reader_set_value(int id,int value) {
    static const int low[]={0,16,110,0,0,0,0};
    static const int high[]={4,34,190,30,1,24,20};
    if(id<0 || id>=7 || value<low[id] || value>high[id])return 0;
    switch(id) {
    case 0:options[OPT_FONT]=value;break;
    case 1:settings.size=value;break;
    case 2:settings.leading=value;break;
    case 3:settings.margin=value;break;
    case 4:options[OPT_ALIGN]=value;break;
    case 5:options[OPT_PARAGRAPH]=value;break;
    case 6:options[OPT_LETTERSPACE]=value;break;
    }
    apply_reading_settings();
    return 1;
}
typedef struct {int count;DocumentMention matches[DOCUMENT_MENTION_LIMIT];} DocumentExcerpt;
static struct {pid_t child;int fd,action;int64_t due;size_t received;DocumentExcerpt result;} document_excerpt={.fd=-1};
static void document_excerpt_close(void) {
    if(document_excerpt.fd>=0)close(document_excerpt.fd);
    if(document_excerpt.child>0) {
        kill(document_excerpt.child,SIGKILL);
        while(waitpid(document_excerpt.child,NULL,0)<0 && errno==EINTR){}
    }
    document_excerpt.fd=-1;document_excerpt.child=0;document_excerpt.received=0;
}
static int document_excerpt_start(const char *term,int action) {
    document_excerpt_close();
    if(current<0 || !*term || strlen(term)>128)return package_acknowledge(action,0);
    int out[2];
    if(pipe(out))return package_acknowledge(action,0);
    pid_t child=fork();
    if(child==0) {
        close(out[0]);
        app_process_detach_inherited();
        DocumentExcerpt result={0};
        result.count=engine_mentions(term,page,result.matches,DOCUMENT_MENTION_LIMIT);
        ssize_t written=write(out[1],&result,sizeof(result));
        _exit(written==(ssize_t)sizeof(result) ? 0 : 1);
    }
    close(out[1]);
    if(child<0){close(out[0]);return package_acknowledge(action,0);}
    fcntl(out[0],F_SETFL,O_NONBLOCK);
    document_excerpt.child=child;document_excerpt.fd=out[0];
    document_excerpt.action=action;document_excerpt.due=extra_millis()+8000;
    return 1;
}
static int document_excerpt_wait(void) {return document_excerpt.child>0 ? 100 : -1;}
static void document_excerpt_poll(void) {
    if(document_excerpt.child<=0)return;
    DocumentExcerpt *pending=&document_excerpt.result;
    size_t offset=document_excerpt.received;
    ssize_t got=read(document_excerpt.fd,(char *)pending+offset,sizeof(*pending)-offset);
    if(got>0)document_excerpt.received+=(size_t)got;
    if(document_excerpt.received<sizeof(*pending) &&
       (got>0 || (got<0 && (errno==EAGAIN || errno==EINTR))) &&
       extra_millis()<document_excerpt.due)return;
    DocumentExcerpt result=*pending;
    int ok=document_excerpt.received==sizeof(result) && result.count>0 && result.count<=DOCUMENT_MENTION_LIMIT;
    int action=document_excerpt.action;
    document_excerpt_close();
    if(!ok){package_acknowledge(action,0);return;}
    char *message=NULL;size_t length=0;
    FILE *out=package_reply(&message,&length,action,1);
    if(out) {
        fprintf(out,",\"mention_count\":%d",result.count);
        for(int i=0;i<result.count;i++) {
            char key[20];snprintf(key,sizeof(key),"quote%d",i);
            package_field(out,key,result.matches[i].quote);
            fprintf(out,",\"offset%d\":%d,\"length%d\":%d",i,result.matches[i].offset,i,result.matches[i].length);
        }
        package_reply_end(out,&message);
    }
}
static int document_selection_operation(const char *operation,int action) {
    int ok=current>=0 && reader_ui.selecting==2;
    if(!strcmp(operation,"selection_clear")) {
        reader_ui.selecting=0;
        return package_acknowledge(action,1);
    }
    if(!ok)return package_acknowledge(action,0);
    if(!strcmp(operation,"selection_copy")) {
        snprintf(clipboard_text,sizeof(clipboard_text),"%s",reader_ui.selection.text);
    } else if(!strcmp(operation,"selection_mark")) {
        if(reader_ui.annotation_count>=128)return package_acknowledge(action,0);
        Annotation *annotation=&reader_ui.annotations[reader_ui.annotation_count++];
        memset(annotation,0,sizeof(*annotation));
        engine_position(page,&annotation->position);
        snprintf(annotation->text,sizeof(annotation->text),"%s",reader_ui.selection.text);
        snprintf(annotation->position.quote,sizeof(annotation->position.quote),"%s",reader_ui.selection.text);
        ok=annotations_write();
        if(!ok)reader_ui.annotation_count--;
    } else return package_acknowledge(action,0);
    reader_ui.selecting=0;
    return package_acknowledge(action,ok);
}
static int package_document(const char *operation,const char *data,int action) {
    if(active_app()!=document_provider())return 0;
    if(!strncmp(operation,"selection_",10))return document_selection_operation(operation,action);
    if(!strcmp(operation,"mentions"))return document_excerpt_start(data,action);
    if(!strcmp(operation,"setting")) {
        int field,value;char tail;
        if(sscanf(data,"%d %d %c",&field,&value,&tail)!=2 || !reader_set_value(field,value))
            return package_acknowledge(action,0);
    } else if(!strcmp(operation,"resume_book")) {
        int last=library_last();
        if(last<0)return package_acknowledge(action,0);
        return ui_open(last) || package_acknowledge(action,0);
    } else if(strcmp(operation,"state") && strcmp(operation,"contents")) {
        if(selected<0 || current<0 || !engine_count())return package_acknowledge(action,0);
        if(!strcmp(operation,"bookmark"))toggle_bookmark();
        else {
            char *end;long value=strtol(data,&end,10);
            if(!*data || *end)return package_acknowledge(action,0);
            if(!strcmp(operation,"turn") && (value==-1 || value==1))turn((int)value);
            else if(!strcmp(operation,"goto") && value>=0 && value<engine_count())turn((int)value-page);
            else if(!strcmp(operation,"annotation") && value>=0 && value<reader_ui.annotation_count)
                turn(engine_restore(&reader_ui.annotations[value].position)-page);
            else if(!strcmp(operation,"remove_annotation") && value>=0 && value<reader_ui.annotation_count) {
                int i=(int)value;Annotation saved=reader_ui.annotations[i];
                memmove(reader_ui.annotations+i,reader_ui.annotations+i+1,
                    (size_t)(reader_ui.annotation_count-i-1)*sizeof(saved));
                reader_ui.annotation_count--;
                if(!annotations_write()) {
                    memmove(reader_ui.annotations+i+1,reader_ui.annotations+i,
                        (size_t)(reader_ui.annotation_count-i)*sizeof(saved));
                    reader_ui.annotations[i]=saved;reader_ui.annotation_count++;
                    return package_acknowledge(action,0);
                }
            } else return package_acknowledge(action,0);
        }
    }
    char *message=NULL;size_t length=0;
    FILE *out=package_reply(&message,&length,action,1);
    if(!out)return 0;
    int total=selected>=0 && current>=0 ? engine_count() : 0;
    package_field(out,"title",total>0 ? library[selected].title : "");
    fprintf(out,",\"page\":%d,\"total\":%d",page,total);
    for(int i=0;i<10;i++)fprintf(out,",\"setting%d\":%d",i,reader_value(i));
    if(!strcmp(operation,"contents") && total>0) {
        int category,list_page;char tail;
        if(sscanf(data,"%d %d %c",&category,&list_page,&tail)==2 && category>=0 &&
           category<=2 && list_page>=0 && list_page<=128) {
            int count=category==0 ? reader_ui.chapter_count : category==1 ?
                library[selected].bookmark_count : reader_ui.annotation_count;
            if(list_page*8>=count)list_page=count ? (count-1)/8 : 0;
            fprintf(out,",\"count\":%d,\"list_page\":%d",count,list_page);
            for(int row=0;row<8 && list_page*8+row<count;row++) {
                int i=list_page*8+row,position=category==0 ? reader_ui.chapters[i].page :
                    category==1 ? library[selected].bookmarks[i] : i;
                char key[24],label[40];snprintf(key,sizeof(key),"label%d",row);
                snprintf(label,sizeof(label),"%d",position+1);
                package_field(out,key,category==0 ? reader_ui.chapters[i].title : category==1 ?
                    label : reader_ui.annotations[i].text);
                fprintf(out,",\"position%d\":%d",row,position);
            }
        }
    }
    package_reply_end(out,&message);
    return 1;
}
