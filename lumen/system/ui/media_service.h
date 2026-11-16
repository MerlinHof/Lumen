static int package_photo_index(const char *id) {
    if(strlen(id)!=16 || strspn(id,"0123456789abcdef")!=16)return -1;
    uint64_t key=strtoull(id,NULL,16);
    for(int i=0;i<photo_count;i++)if(photo_id(photo_names[i])==key)return i;
    return -1;
}
static int package_media(const char *operation,const char *data,int action) {
    if(!strcmp(operation,"list")) {
        media_scan();
        char *message=NULL;
        size_t length=0;
        FILE *out=package_reply(&message,&length,action,1);
        if(!out)return 0;
        fputs(",\"ids\":\"",out);
        for(int i=0;i<photo_count;i++)
            fprintf(out,"%016llx%s",(unsigned long long)photo_id(photo_names[i]),i+1<photo_count ? " " : "");
        fputc('"',out);
        package_reply_end(out,&message);
        return 1;
    }
    int i=package_photo_index(data);if(i<0)return package_acknowledge(action,0);
    char path[2048];
    snprintf(path,sizeof(path),"%s/photos/%s",data_root,photo_names[i]);
    int ok=0;
    if(!strcmp(operation,"rotate")) {
        ok=photo_image_rotate(path);
        if(ok)photo_invalidate_image(photo_names[i]);
    } else if(!strcmp(operation,"delete")) {
        ok=!unlink(path);
        if(ok)photo_invalidate_image(photo_names[i]);
    } else if(!strcmp(operation,"lock")) {
        local_path(path,sizeof(path),"lock-photo.txt");
        ok=atomic_write(path,photo_names[i],strlen(photo_names[i]));
        if(ok){options[OPT_LOCK]=3;library_options_save();}
    }
    return package_acknowledge(action,ok);
}
