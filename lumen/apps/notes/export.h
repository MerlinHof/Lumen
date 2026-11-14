/* Transfer delegates reading and formatting of the note collection here. */
static int export_index(const char *root,const char *output) {
    char *json=NULL;size_t size=0;
    FILE *file=open_memstream(&json,&size);
    if(!file)return 0;
    int total=read_note_ids(root);
    fputs("{\"notes\":[",file);
    for(int i=0;i<total;i++) {
        int id=note_ids_local[i];
        char title[240]="";
        read_note_title(root,id,title,sizeof(title));
        fprintf(file,"%s{\"id\":%d,\"title\":",i ? "," : "",id);
        lumen_json_write(file,*title ? title : "Note");
        fprintf(file,",\"created\":%d}",id);
    }
    fputs("]}",file);
    int ok=!fclose(file) && lumen_storage_write_at(output,"body",json,size);
    free(json);
    return ok;
}
static int export_document(const char *root,const char *output,const char *data,int pdf) {
    char *end;
    long id=strtol(data,&end,10);
    if(end==data || *end || id<=0 || id>INT_MAX)return 0;
    char path[64];size_t size;
    unsigned char *content=malloc(65536);
    if(!content)return 0;
    snprintf(path,sizeof(path),"notes/%ld.txt",id);
    int ok=lumen_storage_read_at(root,path,content,65536,&size) &&
        lumen_storage_write_at(output,"body",content,size);
    free(content);
    if(!pdf || !ok)return ok;
    unsigned char *pixels=malloc(NOTE_INK_WIDTH*NOTE_INK_HEIGHT);
    if(!pixels)return 0;
    for(int i=0;ok && i<512;i++) {
        snprintf(path,sizeof(path),"notes/%ld-page-%d.ink",id,i);
        if(lumen_storage_read_at(root,path,pixels,NOTE_INK_WIDTH*NOTE_INK_HEIGHT,&size) &&
           size==NOTE_INK_WIDTH*NOTE_INK_HEIGHT) {
            snprintf(path,sizeof(path),"page-%d.raw",i);
            ok=lumen_storage_write_at(output,path,pixels,size);
        }
    }
    free(pixels);
    return ok;
}
static void serve_note(const char *operation,const char *data) {
    if(strncmp(operation,"export_",7)){serve(operation,data);return;}
    const char *root=getenv("LUMEN_LEGACY_DATA"),*output=getenv("LUMEN_EXPORT_DATA");
    if(!root || !output){lumen_app_service_result(0,"");return;}
    char mime[80]="";
    int index=!strcmp(operation,"export_list");
    int pdf=!strcmp(operation,"export_pdf"),plain=!strcmp(operation,"export_text");
    int ok=0;
    if(index && !*data) {
        ok=export_index(root,output);
        snprintf(mime,sizeof(mime),"json");
    } else if(pdf || plain) {
        ok=export_document(root,output,data,pdf);
        if(ok)snprintf(mime,sizeof(mime),"%s\nnote-%ld.%s",pdf ? "markdown" : "text",
            strtol(data,NULL,10),pdf ? "pdf" : "txt");
    }
    lumen_app_service_result(ok,mime);
}
