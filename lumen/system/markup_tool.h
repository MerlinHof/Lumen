/* A bounded markup tree for standalone tools. Each record contains its depth,
   tag, text, and the values of attributes named by the caller. */
static int markup_field(const char *value) {
    uint32_t size=value ? (uint32_t)strlen(value) : 0;
    return fwrite(&size,sizeof(size),1,stdout)==1 &&
           (!size || fwrite(value,1,size,stdout)==size);
}
static int markup_write(fz_xml *node,uint32_t depth,int count,char **names) {
    for(;node;node=fz_xml_next(node)) {
        if(fwrite(&depth,sizeof(depth),1,stdout)!=1 ||
           !markup_field(fz_xml_tag(node)) || !markup_field(fz_xml_text(node)))return 0;
        for(int i=0;i<count;i++) {
            const char *value=fz_xml_att(node,names[i]);
            if(value && !*value) {
                static const char *const flags[]={"hidden","disabled","defer","async","nomodule","open","checked"};
                for(size_t flag=0;flag<sizeof(flags)/sizeof(*flags);flag++)
                    if(!strcmp(names[i],flags[flag])){value="1";break;}
            }
            if(!markup_field(value))return 0;
        }
        if(depth<100 && !markup_write(fz_xml_down(node),depth+1,count,names))return 0;
    }
    return 1;
}
static int markup_tool(int html,int count,char **names) {
    fz_context *ctx=fz_new_context(NULL,NULL,8<<20);
    if(!ctx)return 0;
    FILE *input=stdin;
    fz_buffer *buffer=NULL;
    fz_xml *xml=NULL;
    int ok=0;
    fz_var(buffer);
    fz_var(xml);
    fz_var(ok);
    fz_try(ctx) {
        buffer=fz_new_buffer(ctx,4096);
        unsigned char bytes[4096];
        size_t total=0,n;
        while((n=fread(bytes,1,sizeof(bytes),input))) {
            total+=n;
            if(total>(4u<<20))fz_throw(ctx,FZ_ERROR_GENERIC,"Markup input exceeds 4 MiB");
            fz_append_data(ctx,buffer,bytes,n);
        }
        if(ferror(input))fz_throw(ctx,FZ_ERROR_GENERIC,"Could not read markup input");
        xml=html ? fz_parse_xml_from_html5(ctx,buffer) : fz_parse_xml(ctx,buffer,0);
        ok=markup_write(fz_xml_root(xml),0,count,names) && !fflush(stdout);
    }
    fz_always(ctx) {
        fz_drop_xml(ctx,xml);
        fz_drop_buffer(ctx,buffer);
    }
    fz_catch(ctx) {ok=0;}
    fz_drop_context(ctx);
    return ok;
}
