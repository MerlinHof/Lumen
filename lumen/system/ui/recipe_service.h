/* Provider/cache operations for independently packaged content clients. */
static int recipe_reply(int action,int ok) {
    char *message=NULL;size_t length=0;
    FILE *out=package_reply(&message,&length,action,ok);
    if(!out)return 0;
    package_reply_end(out,&message);
    return ok;
}
static int package_recipes(const char *operation,const char *data,int action) {
    if(!strcmp(operation,"search"))
        return recipe_reply(action,*data && strlen(data)<256 && job_start16(1,data));
    if(!strcmp(operation,"fetch")) {
        for(int i=0;i<recipes_count17;i++)if(!strcmp(recipes17[i].id,data)) {
            if(job_pid16>0)return recipe_reply(action,0);
            selected_recipe17=recipes17[i];
            return recipe_reply(action,job_start16(4,""));
        }
        return recipe_reply(action,0);
    }
    if(!strcmp(operation,"open")) {
        recipe_archive16();
        for(int i=0;i<recipe_recent_count16;i++)if(!strcmp(recipe_recent16[i].id,data)) {
            RecipeRecent16 *item=&recipe_recent16[i];
            char path[4096];
            snprintf(path,sizeof(path),"%s/recipe-archive16/%s.html",data_root,item->id);
            if(!access(path,R_OK)) {
                int ok=file_open16(path,ui_route());
                if(ok)snprintf(library[BOOK_LIMIT].title,sizeof(library[BOOK_LIMIT].title),"%s",item->title);
                return ok;
            }
            snprintf(path,sizeof(path),"recipe-archive16/%s.txt",item->id);
            char *text=service_read(path,65536);
            if(text) {
                char *line=strchr(text,'\n');
                reader16(item->title,line ? line+1 : text,ui_route());
                free(text);
                return 1;
            }
        }
        return recipe_reply(action,0);
    }
    int search=!strcmp(operation,"results");
    if(!search && strcmp(operation,"archive"))return 0;
    if(!search)recipe_archive16();
    int count=search ? recipes_count17 : recipe_recent_count16;
    char *end;long page=strtol(data,&end,10);
    if(!*data || *end || page<0 || page>128 || page*6>=count)page=0;
    char *message=NULL;size_t length=0;
    FILE *out=package_reply(&message,&length,action,1);
    if(!out)return 0;
    fprintf(out,",\"count\":%d,\"page\":%ld,\"busy\":%d",count,page,
            job_pid16>0 && (job_kind16==1 || job_kind16==4));
    for(int i=0;i<6 && page*6+i<count;i++) {
        int index=(int)page*6+i;
        char field[20],source[64]="";
        const char *id=search ? recipes17[index].id : recipe_recent16[index].id;
        const char *title=search ? recipes17[index].title : recipe_recent16[index].title;
        if(search) {
            if(recipes17[index].meal)snprintf(source,sizeof(source),"TheMealDB");
            else snprintf(source,sizeof(source),"Wikibooks · %s",recipes17[index].lang);
        }
        snprintf(field,sizeof(field),"id%d",i);package_field(out,field,id);
        snprintf(field,sizeof(field),"title%d",i);package_field(out,field,title);
        snprintf(field,sizeof(field),"source%d",i);package_field(out,field,source);
    }
    package_reply_end(out,&message);
    return 1;
}
