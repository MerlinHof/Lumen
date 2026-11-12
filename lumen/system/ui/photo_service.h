static int package_photo_index(const char *id) {
    if(strlen(id)!=16 || strspn(id,"0123456789abcdef")!=16)return -1;
    uint64_t key=strtoull(id,NULL,16);
    for(int i=0;i<photo_count;i++)if(photo_id(photo_names[i])==key)return i;
    return -1;
}
static int package_photos(const char *operation,const char *data,int action) {
    photo_catalog_load();
    if(!strcmp(operation,"album")) {
        int available=-1;
        if(!*data || strlen(data)>=64)return package_acknowledge(action,0);
        for(int i=0;i<PHOTO_FOLDERS;i++) {
            if(!strcmp(data,photo_catalog.folders[i]))return package_acknowledge(action,0);
            if(available<0 && !*photo_catalog.folders[i])available=i;
        }
        if(available<0)return package_acknowledge(action,0);
        snprintf(photo_catalog.folders[available],64,"%s",data);
        if(!photo_catalog_save()){photo_catalog.folders[available][0]=0;return package_acknowledge(action,0);}
        char *message=NULL;size_t length=0;FILE *out=package_reply(&message,&length,action,1);
        if(!out)return 0;
        fprintf(out,",\"folder\":%d",available+1);package_reply_end(out,&message);return 1;
    }
    if(!strcmp(operation,"duration") || !strcmp(operation,"fill") || !strcmp(operation,"landscape")) {
        char *end;long value=strtol(data,&end,10);
        if(!*data || *end)return package_acknowledge(action,0);
        int duration=!strcmp(operation,"duration");
        if(duration ? value<5 || value>120 : value<0 || value>1)return package_acknowledge(action,0);
        int *setting=duration ? &photo_catalog.seconds : !strcmp(operation,"fill") ?
            &photo_catalog.fill : &photo_catalog.landscape;
        int previous=*setting;*setting=(int)value;
        if(!photo_catalog_save()){*setting=previous;return package_acknowledge(action,0);}
        return package_acknowledge(action,1);
    }
    if(!strcmp(operation,"list")) {
        int folder,page;char tail;
        if(sscanf(data,"%d %d %c",&folder,&page,&tail)!=2 ||
           folder<0 || folder>PHOTO_FOLDERS || page<0 || page>512)return package_acknowledge(action,0);
        photo_folder=folder && *photo_catalog.folders[folder-1] ? folder : 0;
        v1_photos_scan();
        if(page*16>=photo_count)page=photo_count ? (photo_count-1)/16 : 0;
        char *message=NULL;size_t length=0;FILE *out=package_reply(&message,&length,action,1);
        if(!out)return 0;
        fprintf(out,",\"folder\":%d,\"page\":%d,\"count\":%d,\"duration\":%d,\"fill\":%d,\"landscape\":%d",
            photo_folder,page,photo_count,photo_catalog.seconds,photo_catalog.fill,photo_catalog.landscape);
        for(int i=0;i<PHOTO_FOLDERS;i++) {
            char field[20];snprintf(field,sizeof(field),"folder%d",i+1);package_field(out,field,photo_catalog.folders[i]);
        }
        for(int i=0;i<16 && page*16+i<photo_count;i++) {
            char key[20],field[24];uint64_t id=photo_id(photo_names[page*16+i]);
            snprintf(key,sizeof(key),"%016llx",(unsigned long long)id);
            snprintf(field,sizeof(field),"id%d",i);package_field(out,field,key);
            PhotoMembership *member=photo_membership(id,0);
            fprintf(out,",\"albums%d\":%u",i,member ? member->folders : 0);
        }
        package_reply_end(out,&message);
        return 1;
    }
    char id[17];int folder=0;
    if(sscanf(data,"%16[0-9a-f] %d",id,&folder)<1)return package_acknowledge(action,0);
    int i=package_photo_index(id);if(i<0)return package_acknowledge(action,0);
    char path[2048];
    snprintf(path,sizeof(path),"%s/photos/%s",data_root,photo_names[i]);
    int ok=0;
    if(!strcmp(operation,"rotate")) {
        ok=photo_image_rotate(path);
        if(ok)photo_invalidate_image(photo_names[i]);
    } else if(!strcmp(operation,"delete")) {
        ok=!unlink(path);
        if(ok) {
            photo_invalidate_image(photo_names[i]);
            PhotoMembership *member=photo_membership(photo_id(photo_names[i]),0);
            if(member){*member=(PhotoMembership){0};photo_catalog_save();}
        }
    } else if(!strcmp(operation,"lock")) {
        local_path(path,sizeof(path),"lock-photo.txt");
        ok=atomic_write(path,photo_names[i],strlen(photo_names[i]));
        if(ok){options[OPT_LOCK]=3;library_options_save();}
    } else if(!strcmp(operation,"assign") && folder>0 && folder<=PHOTO_FOLDERS && *photo_catalog.folders[folder-1]) {
        uint64_t key=photo_id(photo_names[i]);PhotoMembership *member=photo_membership(key,1);
        if(member) {
            PhotoMembership old=*member;
            member->id=key;member->folders^=1u<<(folder-1);
            if(!member->folders)member->id=0;
            ok=photo_catalog_save();if(!ok)*member=old;
        }
    }
    return package_acknowledge(action,ok);
}
