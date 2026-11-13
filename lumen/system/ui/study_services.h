/* Study uploads are user documents. Packages get names and scoped open
 * requests, without receiving device filesystem paths. */
static int study_document_path(char path[4096],const char *name) {
    if(!*name || name[0]=='.' || strlen(name)>255 || strchr(name,'/') || strchr(name,'\\'))return 0;
    int n=snprintf(path,4096,"%s/study/%s",data_root,name);
    struct stat info;
    return n>0 && n<4096 && !lstat(path,&info) && S_ISREG(info.st_mode);
}
static int package_study_material(const char *operation,const char *data,int action) {
    if(strcmp(operation,"open") || app_process_owner()!=APP_STUDY)return package_acknowledge(action,0);
    char path[4096];
    if(study_document_path(path,data) && file_open16(path,ui_route()))return 1;
    return package_acknowledge(action,0);
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
