/* The sender supplies a temporary EPUB or PDF in its own app data. */
static int package_book_import(const char *file,const char *title) {
    int app=app_process_owner();
    if(app<0 || app!=active_app())return 0;
    char leaf[256];
    int dir=lumen_storage_parent_at(app_process_data_dir(),file,0,leaf);
    if(dir<0)return 0;
    int in=openat(dir,leaf,O_RDONLY|O_NONBLOCK|O_NOFOLLOW|O_CLOEXEC);
    if(in<0){close(dir);return 0;}
    struct stat st;
    const char *extension=strrchr(leaf,'.');
    int epub=extension && !strcmp(extension,".epub"),pdf=extension && !strcmp(extension,".pdf");
    unsigned char magic[5];
    int ok=(epub || pdf) && !fstat(in,&st) && S_ISREG(st.st_mode) &&
        st.st_size>=5 && st.st_size<=(128u<<20) && read(in,magic,sizeof(magic))==sizeof(magic) &&
        (pdf ? !memcmp(magic,"%PDF-",5) : !memcmp(magic,"PK\3\4",4));
    close(in);
    if(!ok){close(dir);return 0;}
    char name[192];
    size_t length=0;
    while(title[length] && length<170) {
        unsigned char ch=(unsigned char)title[length];
        name[length++]=ch<32 || strchr("/\\:*?\"<>|",ch) ? '_' : (char)ch;
    }
    while(length && ((unsigned char)title[length]&192)==128)length--;
    while(length && (name[length-1]=='.' || name[length-1]==' '))length--;
    if(!length || name[0]=='.'){memcpy(name,"Book",4);length=4;}
    snprintf(name+length,sizeof(name)-length,"%s",extension);
    int library_dir=open(library_root,O_RDONLY|O_DIRECTORY|O_CLOEXEC);
    if(library_dir<0){close(dir);return 0;}
    int reserve=openat(library_dir,name,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);
    ok=reserve>=0;
    if(ok) {
        close(reserve);
        ok=!renameat(dir,leaf,library_dir,name);
        if(!ok)unlinkat(library_dir,name,0);
        else {fsync(library_dir);fsync(dir);}
    }
    close(library_dir);close(dir);
    if(!ok)return 0;
    library_dirty=1;
    if(active_app()!=document_provider())refresh_library();
    ui_message("Buch zur Bibliothek hinzugefügt.");
    return 1;
}
