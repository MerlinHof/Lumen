/* Study uploads are user documents. Packages get names and scoped open
 * requests, without receiving device filesystem paths. */
static int study_document_path(char path[4096],const char *name) {
    if(!*name || name[0]=='.' || strlen(name)>255 || strchr(name,'/') || strchr(name,'\\'))return 0;
    int n=snprintf(path,4096,"%s/study/%s",data_root,name);
    struct stat info;
    return n>0 && n<4096 && !lstat(path,&info) && S_ISREG(info.st_mode);
}
static int study_compare(const void *a,const void *b){return strcasecmp(a,b);}
static int package_study_material(const char *operation,const char *data,int action) {
    if(!strcmp(operation,"open")) {
        char path[4096];
        return study_document_path(path,data) && file_open16(path,ui_route());
    }
    if(strcmp(operation,"list"))return 0;
    char *end;long requested=strtol(data,&end,10);
    if(!*data || *end || requested<0 || requested>=512)requested=0;
    char path[4096];snprintf(path,sizeof(path),"%s/study",data_root);
    DIR *folder=opendir(path);
    static char names[512][256];
    int count=0;struct dirent *item;
    while(folder && count<512 && (item=readdir(folder)))
        if(study_document_path(path,item->d_name))snprintf(names[count++],256,"%s",item->d_name);
    if(folder)closedir(folder);
    qsort(names,count,sizeof(*names),study_compare);
    if(requested*5>=count)requested=0;
    char *message=NULL;size_t length=0;
    FILE *out=package_reply(&message,&length,action,1);
    if(!out)return 0;
    fprintf(out,",\"page\":%ld,\"count\":%d",requested,count);
    for(int i=0;i<5 && requested*5+i<count;i++) {
        char key[20];snprintf(key,sizeof(key),"name%d",i);
        package_field(out,key,names[requested*5+i]);
    }
    package_reply_end(out,&message);
    return 1;
}
static int package_learning_note(const char *data,int action) {
    const char *split=strchr(data,'\n');
    if(!split || split-data>255 || !split[1])return 0;
    char title[256];snprintf(title,sizeof(title),"%.*s",(int)(split-data),data);
    u16_notes_defaults();
    int folder=0;
    for(int i=0;i<note_tree.nf;i++)
        if(!strcmp(note_tree.folder[i].name,"Learning"))folder=note_tree.folder[i].id;
    char path[4096],content[9000];
    int id=(int)time(NULL);
    do {snprintf(path,sizeof(path),"%s/notes/%d.txt",data_root,id++);} while(!access(path,F_OK));
    snprintf(content,sizeof(content),tr("# Abrufübung: %s\n\n%s"),*title ? title : tr("Lernen"),split+1);
    int ok=atomic_write(path,content,strlen(content));
    if(ok){v1_place(id-1,folder);v1_save();notes_scan();}
    char *message=NULL;size_t length=0;
    FILE *out=package_reply(&message,&length,action,ok);
    if(out)package_reply_end(out,&message);
    return ok;
}
static int package_start_timer(const char *data,int action) {
    char *end;long minutes=strtol(data,&end,10);
    if(!*data || *end || minutes<1 || minutes>240)return 0;
    timer_minutes=(int)minutes;
    timer_total_seconds=timer_minutes*60;
    timer_due=extra_millis()+timer_total_seconds*1000LL;
    timer_paused=0;
    extra_save();
    char *message=NULL;size_t length=0;
    FILE *out=package_reply(&message,&length,action,1);
    if(out)package_reply_end(out,&message);
    return 1;
}
