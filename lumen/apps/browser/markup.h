/* Translate web documents to a static HTML subset with bounded local resources. */
enum { WEB_URL=1024, WEB_BODY=2<<20, WEB_CSS=64<<10, WEB_IMAGE=256<<10 };
static const char *const attributes[]={"href","rel","class","id","style","alt","src"};
#define WEB_ATTRIBUTES (sizeof(attributes)/sizeof(attributes[0]))
static int url_valid(const char *url) {
    if(strncmp(url,"http://",7) && strncmp(url,"https://",8))return 0;
    const char *host=strstr(url,"://")+3;
    size_t host_n=strcspn(host,"/?#");
    if(!host_n || memchr(host,'@',host_n) || strlen(url)>=WEB_URL)return 0;
    for(const unsigned char *p=(const unsigned char *)url;*p;p++)
        if(*p<=32 || *p==127 || strchr("\\\"<>",*p))return 0;
    return 1;
}
static int url_join(const char *base,const char *ref,char out[WEB_URL]) {
    if(!ref || !*ref)return 0;
    const char *scheme=strstr(base,"://");
    if(!scheme)return 0;
    const char *host=scheme+3;
    const char *path=host+strcspn(host,"/?#");
    int size;
    if(strstr(ref,"://"))size=snprintf(out,WEB_URL,"%s",ref);
    else if(!strncmp(ref,"//",2))size=snprintf(out,WEB_URL,"%.*s:%s",(int)(scheme-base),base,ref);
    else if(*ref=='/')size=snprintf(out,WEB_URL,"%.*s%s",(int)(path-base),base,ref);
    else if(*ref=='#')size=snprintf(out,WEB_URL,"%.*s%s",(int)strcspn(base,"#"),base,ref);
    else if(*ref=='?')size=snprintf(out,WEB_URL,"%.*s%s",(int)strcspn(base,"?#"),base,ref);
    else {
        size_t first=strcspn(ref,"/?#");
        if(memchr(ref,':',first))return 0;
        const char *end=path+strcspn(path,"?#");
        while(end>path && end[-1]!='/')end--;
        size=snprintf(out,WEB_URL,"%.*s%s%s",(int)(end-base),base,end==path ? "/" : "",ref);
    }
    return size>0 && size<WEB_URL && url_valid(out);
}
static int same_origin(const char *one,const char *two) {
    size_t a=(size_t)(strstr(one,"://")+3-one),b=(size_t)(strstr(two,"://")+3-two);
    a+=strcspn(one+a,"/?#");b+=strcspn(two+b,"/?#");
    return a==b && !strncasecmp(one,two,a);
}
static void web_escape(FILE *out,const char *text) {
    for(const unsigned char *p=(const unsigned char *)text;*p;p++) {
        if(*p=='&')fputs("&amp;",out);
        else if(*p=='<')fputs("&lt;",out);
        else if(*p=='>')fputs("&gt;",out);
        else if(*p=='"')fputs("&quot;",out);
        else fputc(*p,out);
    }
}
static int permitted(const char *s,size_t len,const char *const *set,size_t count) {
    for(size_t i=0;i<count;i++)if(strlen(set[i])==len && !strncasecmp(s,set[i],len))return 1;
    return 0;
}
static int css_chars(const char *start,const char *end,const char *extra) {
    for(;start<end;start++)if(!isalnum((unsigned char)*start) && !isspace((unsigned char)*start) &&
                           !strchr(extra,*start))return 0;
    return 1;
}
static void css_declarations(FILE *out,const char *p,const char *end) {
    static const char *const names[]={
        "color","background-color","font-family","font-size","font-style","font-weight",
        "line-height","letter-spacing","text-align","text-decoration","white-space",
        "margin","margin-top","margin-bottom","margin-left","margin-right","padding",
        "padding-top","padding-bottom","padding-left","padding-right","display",
        "list-style-type","border","border-top","border-bottom","border-color","border-width",
        "width","max-width","height","vertical-align"
    };
    while(p<end) {
        while(p<end && (isspace((unsigned char)*p) || *p==';'))p++;
        const char *stop=p;
        while(stop<end && *stop!=';')stop++;
        const char *colon=memchr(p,':',(size_t)(stop-p));
        if(colon) {
            const char *name_end=colon,*value=colon+1,*value_end=stop;
            while(name_end>p && isspace((unsigned char)name_end[-1]))name_end--;
            while(value<stop && isspace((unsigned char)*value))value++;
            while(value_end>value && isspace((unsigned char)value_end[-1]))value_end--;
            if(value<value_end && value_end-value<=96 &&
               permitted(p,(size_t)(name_end-p),names,sizeof(names)/sizeof(*names)) &&
               css_chars(value,value_end,"#%.,-")) {
                for(const char *q=p;q<name_end;q++)fputc(tolower((unsigned char)*q),out);
                fprintf(out,":%.*s;",(int)(value_end-value),value);
            }
        }
        p=stop<end ? stop+1 : end;
    }
}
static void css_sheet(FILE *out,const char *css) {
    size_t length=strlen(css);
    if(length>WEB_CSS)return;
    char *clean=malloc(length+1);
    if(!clean)return;
    size_t used=0;
    for(size_t i=0;i<length;i++) {
        if(css[i]=='/' && css[i+1]=='*') {
            const char *end=strstr(css+i+2,"*/");
            if(!end)break;
            i=(size_t)(end-css)+1;
        } else clean[used++]=css[i];
    }
    clean[used]=0;
    const char *p=clean;
    while(*p) {
        while(isspace((unsigned char)*p))p++;
        const char *open=strchr(p,'{'),*semicolon=strchr(p,';');
        if(!open)break;
        if(*p=='@' && semicolon && semicolon<open){p=semicolon+1;continue;}
        const char *close=open+1;
        int depth=1;
        while(*close && depth) {
            if(*close=='{')depth++;
            else if(*close=='}')depth--;
            if(depth)close++;
        }
        if(depth)break;
        if(*p!='@' && open>p && open-p<180 && css_chars(p,open,"#.*,_->+")) {
            fprintf(out,"%.*s{",(int)(open-p),p);
            css_declarations(out,open+1,close);
            fputs("}\n",out);
        }
        p=close+1;
    }
    free(clean);
}
static void web_styles(FILE *out,MarkupNode *node,const char *base,char title[128],int *requests) {
    for(;node;node=node->next) {
        if(!node->tag)continue;
        if(!strcmp(node->tag,"title") && node->child && node->child->text && !*title) {
            size_t length=strlen(node->child->text);
            if(length>127)length=127;
            while(length && ((unsigned char)node->child->text[length]&192)==128)length--;
            snprintf(title,128,"%.*s",(int)length,node->child->text);
            for(char *p=title;*p;p++)if((unsigned char)*p<32)*p=' ';
        } else if(!strcmp(node->tag,"style")) {
            for(MarkupNode *child=node->child;child;child=child->next)
                if(child->text)css_sheet(out,child->text);
        } else if(!strcmp(node->tag,"link") && node->attribute[1] &&
                  !strcasecmp(node->attribute[1],"stylesheet") && *requests<3) {
            char url[WEB_URL];
            if(url_join(base,node->attribute[0],url) && same_origin(base,url)) {
                (*requests)++;
                char *css=malloc(WEB_CSS+1);
                if(css && lumen_http_text((LumenHttpRequest){.url=url,.timeout=5,
                    .allow_http=!strncmp(url,"http://",7),.compressed=1},css,WEB_CSS+1))
                    css_sheet(out,css);
                free(css);
            }
        }
        web_styles(out,node->child,base,title,requests);
    }
}
static const char *image_type(const unsigned char *data,size_t length,int *width,int *height) {
    if(length>=24 && !memcmp(data,"\211PNG\r\n\032\n",8) && !memcmp(data+12,"IHDR",4)) {
        if(data[16] || data[17] || data[20] || data[21])return NULL;
        *width=data[18]*256+data[19];*height=data[22]*256+data[23];
        return "png";
    }
    if(length<4 || data[0]!=255 || data[1]!=216)return NULL;
    for(size_t at=2;at+3<length;) {
        if(data[at++]!=255)return NULL;
        while(at<length && data[at]==255)at++;
        if(at+2>=length)return NULL;
        unsigned marker=data[at++];
        if(marker==0xda || marker==0xd9)return NULL;
        size_t size=data[at]*256+data[at+1];
        if(size<2 || size>length-at)return NULL;
        if((marker==0xc0 || marker==0xc1 || marker==0xc2) && size>=8) {
            *height=data[at+3]*256+data[at+4];
            *width=data[at+5]*256+data[at+6];
            return "jpeg";
        }
        at+=size;
    }
    return NULL;
}
static int web_image(FILE *out,const char *base,const char *src,const char *alt,int *remaining) {
    char url[WEB_URL];
    if(!*remaining || !url_join(base,src,url) || !same_origin(base,url))return 0;
    --*remaining;
    FILE *file=tmpfile();
    if(!file)return 0;
    unsigned char *data=malloc(WEB_IMAGE);
    size_t length=0;
    if(data && lumen_http_download((LumenHttpRequest){.url=url,.timeout=4,
       .allow_http=!strncmp(url,"http://",7)},fileno(file),WEB_IMAGE))
        length=fread(data,1,WEB_IMAGE,file);
    fclose(file);
    int width=0,height=0;
    const char *type=length ? image_type(data,length,&width,&height) : NULL;
    int valid=type && width>0 && width<=1600 && height>0 && height<=1600;
    if(valid) {
        static const char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        fprintf(out,"<img style=\"width:%dpx\" src=\"data:image/%s;base64,",width>496 ? 496 : width,type);
        for(size_t i=0;i<length;i+=3) {
            unsigned a=data[i],b=i+1<length ? data[i+1] : 0,c=i+2<length ? data[i+2] : 0;
            fputc(alphabet[a>>2],out);fputc(alphabet[((a&3)<<4)|(b>>4)],out);
            fputc(i+1<length ? alphabet[((b&15)<<2)|(c>>6)] : '=',out);
            fputc(i+2<length ? alphabet[c&63] : '=',out);
        }
        fputs("\" alt=\"",out);
        if(alt)web_escape(out,alt);
        fputs("\">",out);
    }
    free(data);
    return valid;
}
static void web_content(FILE *out,MarkupNode *node,const char *base,int *images) {
    static const char *const allowed[]={"a","article","aside","b","blockquote","br","code","dd","div","dl","dt",
        "em","footer","h1","h2","h3","h4","h5","h6","header","hr","i","li","main","nav","ol","p","pre",
        "section","small","span","strong","sub","sup","table","tbody","td","th","thead","tr","u","ul"};
    static const char *const omit[]={"head","script","style","template","svg","math","iframe","video",
        "audio","noscript","select","textarea"};
    for(;node;node=node->next) {
        if(node->text){web_escape(out,node->text);continue;}
        const char *tag=node->tag;
        if(!tag || permitted(tag,strlen(tag),omit,sizeof(omit)/sizeof(*omit)))continue;
        if(!strcmp(tag,"img")) {
            if(!web_image(out,base,node->attribute[6],node->attribute[5],images) &&
               node->attribute[5] && *node->attribute[5]) {
                fputs("<span>[",out);web_escape(out,node->attribute[5]);fputs("]</span>",out);
            }
            continue;
        }
        int present=permitted(tag,strlen(tag),allowed,sizeof(allowed)/sizeof(*allowed));
        int single=present && (!strcmp(tag,"br") || !strcmp(tag,"hr"));
        if(present) {
            fprintf(out,"<%s",tag);
            if(!strcmp(tag,"a")) {
                char url[WEB_URL];
                const char *ref=node->attribute[0];
                if(url_join(base,ref,url)) {
                    fputs(" href=\"",out);web_escape(out,*ref=='#' ? ref : url);fputc('"',out);
                }
            }
            for(int field=2;field<=3;field++)if(node->attribute[field]) {
                fprintf(out," %s=\"",attributes[field]);web_escape(out,node->attribute[field]);fputc('"',out);
            }
            if(node->attribute[4]) {
                const char *style=node->attribute[4];
                fputs(" style=\"",out);css_declarations(out,style,style+strlen(style));fputc('"',out);
            }
            fputc('>',out);
        }
        web_content(out,node->child,base,images);
        if(present && !single)fprintf(out,"</%s>",tag);
    }
}
static int web_document(const char *source,const char *clean,const char *url,char title[128]) {
    MarkupNode *root=markup_load(source,1,attributes,WEB_ATTRIBUTES);
    if(!root)return 0;
    FILE *out=fopen(clean,"wb");
    if(!out){markup_free(root);return 0;}
    fputs("<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><style>"
          "@page{margin:12px 6px;}body{font-family:sans-serif;line-height:1.35;color:#111;}"
          "a{color:#222;text-decoration:underline;}pre{white-space:pre-wrap;}"
          "h1,h2,h3{line-height:1.18;}td,th{padding:4px;}",out);
    int requests=0;
    web_styles(out,root,url,title,&requests);
    fputs("</style></head><body>",out);
    int images=3;
    web_content(out,root,url,&images);
    fputs("</body></html>",out);
    int ok=!ferror(out);
    if(fclose(out))ok=0;
    markup_free(root);
    return ok;
}
