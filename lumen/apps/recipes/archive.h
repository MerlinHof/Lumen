static pid_t recipe_worker;
static int fetching,open_on_finish;
static char fetched_id[64];
typedef struct {char id[64],title[256];time_t edited;int legacy;} SavedRecipe;
static SavedRecipe saved[128];
static int saved_count;
static const char *const archive_dir="recipe-archive16";

static int valid_recipe_id(const char *id) {
    size_t n=strlen(id);
    return n>0 && n<64 && strspn(id,"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-")==n;
}
static void archive_path(char path[128],const char *id,const char *ext) {
    snprintf(path,128,"%s/%s.%s",archive_dir,id,ext);
}
static void scan_recipes(const char *root,int legacy) {
    if(!root || !*root)return;
    char dir[4096];
    if(snprintf(dir,sizeof(dir),"%s/%s",root,archive_dir)>=(int)sizeof(dir))return;
    DIR *d=opendir(dir);
    if(!d)return;
    struct dirent *entry;
    while((entry=readdir(d)) && saved_count<128) {
        const char *ext=strrchr(entry->d_name,'.');
        if(!ext || strcmp(ext,".txt") || ext-entry->d_name>=64)continue;
        char id[64],path[128];
        snprintf(id,sizeof(id),"%.*s",(int)(ext-entry->d_name),entry->d_name);
        if(!valid_recipe_id(id))continue;
        int i=0;
        while(i<saved_count && strcmp(saved[i].id,id))i++;
        if(i<saved_count)continue;
        archive_path(path,id,"txt");
        char *text=read_text(root,path,65536);
        if(!text)continue;
        text[strcspn(text,"\r\n")]=0;
        SavedRecipe *record=&saved[saved_count++];
        record->legacy=legacy;
        snprintf(record->id,sizeof(record->id),"%s",id);
        snprintf(record->title,sizeof(record->title),"%.*s",255,text);
        struct stat info;
        record->edited=!fstatat(dirfd(d),entry->d_name,&info,AT_SYMLINK_NOFOLLOW) ? info.st_mtime : 0;
        free(text);
    }
    closedir(d);
}
static int recently_saved(const void *a,const void *b) {
    const SavedRecipe *left=a,*right=b;
    if(left->edited!=right->edited)return left->edited>right->edited ? -1 : 1;
    return strcmp(left->title,right->title);
}
static void rescan(void) {
    saved_count=0;
    scan_recipes(getenv("LUMEN_APP_DATA"),0);
    scan_recipes(getenv("LUMEN_LEGACY_DATA"),1);
    qsort(saved,saved_count,sizeof(saved[0]),recently_saved);
}
static int import_file(const char *root,const char *path,size_t limit) {
    unsigned char *bytes=malloc(limit);
    if(!bytes)return 0;
    size_t length;
    int ok=lumen_storage_read_at(root,path,bytes,limit,&length) && lumen_storage_write(path,bytes,length);
    free(bytes);
    return ok;
}
static int read_recipe(const char *id,const char *title,int legacy) {
    char path[128];
    archive_path(path,id,"html");
    if(legacy && !lumen_storage_exists(path)) {
        const char *root=getenv("LUMEN_LEGACY_DATA");
        if(import_file(root,path,6000000)) {
            archive_path(path,id,"pgm");
            import_file(root,path,2097152);
            archive_path(path,id,"html");
        } else {
            archive_path(path,id,"txt");
            char *text=read_text(root,path,65536);
            if(!text)return 0;
            char *body=strchr(text,'\n');
            lumen_request("reader",title,body ? body+1 : text,0);
            free(text);
            return 1;
        }
    }
    if(!lumen_storage_exists(path))return 0;
    lumen_reader_document_title(path,NULL,title);
    return 1;
}
static int start_recipe(const char *term,const Recipe17 *recipe) {
    if(recipe_worker>0)return 0;
    if(recipe) {
        selected_recipe17=*recipe;
        snprintf(fetched_id,sizeof(fetched_id),"%s",recipe->id);
    }
    pid_t child=fork();
    if(!child) {
        int null=open("/dev/null",O_RDWR);
        if(null<0 || dup2(null,STDIN_FILENO)<0 || dup2(null,STDOUT_FILENO)<0)_exit(1);
        if(null>STDOUT_FILENO)close(null);
        if(recipe)_exit(recipes_fetch17("") ? 0 : 1);
        recipes_search17(term);
        recipes_parse17();
        _exit(lumen_storage_write("results.bin",recipes17,recipes_count17*sizeof(recipes17[0])) ? 0 : 1);
    }
    if(child<0)return 0;
    recipe_worker=child;
    fetching=recipe!=NULL;
    open_on_finish=1;
    return 1;
}
static int finish_recipe(void) {
    if(recipe_worker<=0)return 0;
    int status;
    pid_t done=waitpid(recipe_worker,&status,WNOHANG);
    if(!done || (done<0 && errno==EINTR))return 0;
    recipe_worker=0;
    if(done<0 || !WIFEXITED(status) || WEXITSTATUS(status))return -1;
    if(fetching) {
        if(open_on_finish && !read_recipe(fetched_id,selected_recipe17.title,0))return -1;
        return 1;
    }
    size_t size;
    if(lumen_storage_read("results.bin",recipes17,sizeof(recipes17),&size))
        recipes_count17=(int)(size/sizeof(recipes17[0]));
    return 1;
}
