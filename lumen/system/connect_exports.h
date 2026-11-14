/* Authenticated package exports. Each response has an isolated work folder. */
static void send_export_file(int fd,const char *path,const char *type,const char *name) {
    int in=open(path,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
    struct stat info;
    if(in<0 || fstat(in,&info) || !S_ISREG(info.st_mode)) {
        if(in>=0)close(in);
        response(fd,404,"text/plain","Export unavailable");
        return;
    }
    char header[512],disposition[160]="";
    if(*name)snprintf(disposition,sizeof(disposition),"Content-Disposition: attachment; filename=\"%s\"\r\n",name);
    int size=snprintf(header,sizeof(header),
        "HTTP/1.1 200 OK\r\nContent-Type: %s\r\n%sContent-Length: %llu\r\n"
        "Cache-Control: no-store\r\nConnection: close\r\n\r\n",
        type,disposition,(unsigned long long)info.st_size);
    if(writeall(fd,header,size)) {
        char bytes[32768];ssize_t n;
        while((n=read(in,bytes,sizeof(bytes)))>0 && writeall(fd,bytes,(size_t)n)){}
    }
    close(in);
}
static int render_export_pdf(const char *folder,const char *body,const char *pdf) {
    int in=open(body,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
    if(in<0)return 0;
    struct stat info;
    if(fstat(in,&info) || !S_ISREG(info.st_mode) || info.st_size<0 || info.st_size>65536) {
        close(in);
        return 0;
    }
    FILE *input=fdopen(in,"rb");
    if(!input){close(in);return 0;}
    char *text=calloc(1,65537);
    if(!text){fclose(input);return 0;}
    size_t size=fread(text,1,65536,input);
    int ok=!ferror(input) && !memchr(text,0,size);
    fclose(input);
    char html[4096];
    snprintf(html,sizeof(html),"%s/layout.html",folder);
    int file=open(html,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
    FILE *layout=file>=0 ? fdopen(file,"w") : NULL;
    if(layout && ok) {
        fputs("<!doctype html><meta charset=utf-8><style>body{font-family:serif;margin:54px 40px 58px "
              "60px;line-height:1.45}p{margin:0 0 .5em}h1,h2,h3{margin:.4em "
              "0}pre{white-space:pre-wrap}code{font-family:monospace}</style>",layout);
        markdown_render17(layout,text);
        ok=!fclose(layout) && engine_document_pdf(html,folder,pdf);
    } else {
        if(layout)fclose(layout);
        else if(file>=0)close(file);
        ok=0;
    }
    free(text);
    return ok;
}
static void web_export(int fd,const char *app,const char *operation,const char *data) {
    char folder[]="/tmp/lumen-export-XXXXXX";
    if(!mkdtemp(folder)){response(fd,500,"text/plain","Cannot prepare export");return;}
    char result[128],body[256];
    int ok=app_package_export(app,operation,data,folder,result,sizeof(result));
    char *name=ok ? strchr(result,'\n') : NULL;
    if(name)*name++=0;
    else name="";
    if(strlen(name)>80 || strspn(name,"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.")!=strlen(name))
        ok=0;
    snprintf(body,sizeof(body),"%s/body",folder);
    if(!ok)response(fd,404,"text/plain","Export unavailable");
    else if(!strcmp(result,"json"))send_export_file(fd,body,"application/json; charset=utf-8",name);
    else if(!strcmp(result,"text"))send_export_file(fd,body,"text/plain; charset=utf-8",name);
    else if(!strcmp(result,"markdown")) {
        char pdf[256];snprintf(pdf,sizeof(pdf),"%s/result.pdf",folder);
        if(render_export_pdf(folder,body,pdf))send_export_file(fd,pdf,"application/pdf",name);
        else response(fd,500,"text/plain","PDF export failed");
    } else response(fd,500,"text/plain","Unsupported export format");
    package_remove(folder);
}
