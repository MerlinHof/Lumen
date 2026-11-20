/* Translate web documents to a static HTML subset with bounded local resources. */
enum { WEB_URL=1024, WEB_BODY=2<<20, WEB_CSS=64<<10, WEB_IMAGE=256<<10 };
static const char *const attributes[]={"href","rel","class","id","style","alt","src","onclick","type","hidden","value","media","name","action","method","placeholder","disabled","formaction","enctype","aria-label","title","oninput","onchange","onsubmit"};
#define WEB_ATTRIBUTES (sizeof(attributes)/sizeof(attributes[0]))
enum { ATTR_HREF=0, ATTR_NAME=12, ATTR_ACTION, ATTR_METHOD, ATTR_PLACEHOLDER, ATTR_DISABLED, ATTR_FORM_ACTION, ATTR_ENCTYPE, ATTR_ARIA_LABEL };
enum { WEB_EDIT, WEB_CLICK, WEB_SUBMIT, WEB_SEARCH, WEB_FIELDS=40, WEB_FIELD_TEXT=256 };
typedef struct {int id,kind;char value[WEB_FIELD_TEXT];} WebAction;
typedef struct {int id,submit;char label[80],value[WEB_FIELD_TEXT];} WebField;
static const char mobile_agent[]="Mozilla/5.0 (Linux; Android 11; Mobile) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/109.0.0.0 Mobile Safari/537.36";
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
static int web_resource_url(const char *base,const char *reference,char out[WEB_URL]) {
    return url_join(base,reference,out) &&
        (strncmp(base,"https://",8) || !strncmp(out,"https://",8));
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
#include "responsive.h"
#include "script.h"

static void css_declarations(FILE *out,const char *p,const char *end) {
    static const char *const names[]={
        "color","background","background-color","font-family","font-size","font-style","font-weight",
        "line-height","letter-spacing","text-align","text-decoration","white-space",
        "margin","margin-top","margin-bottom","margin-left","margin-right","padding",
        "padding-top","padding-bottom","padding-left","padding-right","display",
        "list-style-type","border","border-top","border-bottom","border-color","border-width",
        "width","max-width","min-width","box-sizing","height","max-height","vertical-align","overflow-wrap"
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
                fputc(':',out);
                size_t length=(size_t)(value_end-value);
                if(permitted(p,(size_t)(name_end-p),(const char *const[]){"display"},1) &&
                   (layout_value(value,value_end,"flex") || layout_value(value,value_end,"grid") ||
                    layout_value(value,value_end,"inline-flex") || layout_value(value,value_end,"inline-grid")))
                    fputs("block",out);
                else if(length>=3 && !memcmp(value_end-2,"vw",2)) {
                    char *unit;
                    double width=strtod(value,&unit);
                    if(unit==value_end-2 && width>=0 && width<=200)fprintf(out,"%.1fpx",width*WEB_VIEW_WIDTH/100);
                    else fprintf(out,"%.*s",(int)length,value);
                } else fprintf(out,"%.*s",(int)length,value);
                fputc(';',out);
            }
        }
        p=stop<end ? stop+1 : end;
    }
}
static void css_rules(FILE *out,WebLayout *layout,const char *p,const char *end,int level) {
    if(level>5)return;
    while(p<end) {
        while(p<end && isspace((unsigned char)*p))p++;
        if(p==end)break;
        const char *open=memchr(p,'{',(size_t)(end-p)),*semicolon=memchr(p,';',(size_t)(end-p));
        if(!open)break;
        if(*p=='@' && semicolon && semicolon<open){p=semicolon+1;continue;}
        const char *close=open+1;
        int depth=1;
        while(close<end && depth) {
            if(*close=='{')depth++;
            else if(*close=='}')depth--;
            if(depth)close++;
        }
        if(depth)break;
        if(!strncmp(p,"@media",6) && media_matches(p+6,open))
            css_rules(out,layout,open+1,close,level+1);
        else if(!strncmp(p,"@layer",6))
            css_rules(out,layout,open+1,close,level+1);
        else if(*p!='@' && open>p && open-p<180 && css_chars(p,open,"#.*,_->+:()[]=")) {
            fprintf(out,"%.*s{",(int)(open-p),p);
            css_declarations(out,open+1,close);
            fputs("}\n",out);
            layout_record(layout,p,open,close);
        }
        p=close+1;
    }
}
static void css_sheet(FILE *out,WebLayout *layout,const char *css) {
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
    css_rules(out,layout,clean,clean+used,0);
    free(clean);
}
static void web_styles(FILE *out,WebLayout *layout,MarkupNode *node,const char *base,char title[128],int *requests,int64_t network_end) {
    for(;node;node=node->next) {
        if(!node->tag)continue;
        if(!strcmp(node->tag,"title") && node->child && node->child->text && !*title) {
            size_t length=strlen(node->child->text);
            if(length>127)length=127;
            while(length && ((unsigned char)node->child->text[length]&192)==128)length--;
            snprintf(title,128,"%.*s",(int)length,node->child->text);
            for(char *p=title;*p;p++)if((unsigned char)*p<32)*p=' ';
        } else if(!strcmp(node->tag,"style") && (!node->attribute[11] ||
                  media_matches(node->attribute[11],node->attribute[11]+strlen(node->attribute[11])))) {
            for(MarkupNode *child=node->child;child;child=child->next)
                if(child->text)css_sheet(out,layout,child->text);
        } else if(!strcmp(node->tag,"link") && node->attribute[1] &&
                  !strcasecmp(node->attribute[1],"stylesheet") && *requests<6 &&
                  (!node->attribute[11] || media_matches(node->attribute[11],node->attribute[11]+strlen(node->attribute[11])))) {
            char url[WEB_URL];
            if(web_resource_url(base,node->attribute[0],url) && script_time()<network_end) {
                (*requests)++;
                char *css=malloc(WEB_CSS+1);
                if(css && lumen_http_text((LumenHttpRequest){.url=url,.timeout=4,.redirects=3,.user_agent=mobile_agent,
                    .allow_http=!strncmp(url,"http://",7),.compressed=1},css,WEB_CSS+1))
                    css_sheet(out,layout,css);
                free(css);
            }
        }
        web_styles(out,layout,node->child,base,title,requests,network_end);
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
static int web_image(FILE *out,const char *base,const char *src,const char *alt,int *remaining,int64_t network_end) {
    char url[WEB_URL];
    if(!*remaining || script_time()>=network_end || !web_resource_url(base,src,url))return 0;
    --*remaining;
    FILE *file=tmpfile();
    if(!file)return 0;
    unsigned char *data=malloc(WEB_IMAGE);
    size_t length=0;
    if(data && lumen_http_download((LumenHttpRequest){.url=url,.timeout=4,.redirects=3,.user_agent=mobile_agent,
       .allow_http=!strncmp(url,"http://",7)},fileno(file),WEB_IMAGE))
        length=fread(data,1,WEB_IMAGE,file);
    fclose(file);
    int width=0,height=0;
    const char *type=length ? image_type(data,length,&width,&height) : NULL;
    int valid=type && width>0 && width<=1600 && height>0 && height<=1600;
    if(valid) {
        static const char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        fprintf(out,"<img style=\"width:%dpx\" src=\"data:image/%s;base64,",width>WEB_VIEW_WIDTH-28 ? WEB_VIEW_WIDTH-28 : width,type);
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
static void web_content(FILE *out,MarkupNode *node,const char *base,int *images,
                        const WebLayout *layout,const WebScript *script,int single);
static void web_children(FILE *out,MarkupNode *node,const char *base,int *images,
                         const WebLayout *layout,const WebScript *script) {
    const WebLayoutRule *rule=layout_for(layout,node);
    int children=0;
    for(MarkupNode *p=node->child;p;p=p->next)
        if(p->tag && !p->attribute[9] && strcmp(p->tag,"script") && strcmp(p->tag,"style"))children++;
    int columns=rule ? rule->columns : 0;
    if(columns<0)columns=children>4 ? 2 : children;
    if(columns<2 || children<2) {
        web_content(out,node->child,base,images,layout,script,0);
        return;
    }
    if(columns>children)columns=children;
    fprintf(out,"<table style=\"width:100%%\"><tbody><tr>");
    int index=0;
    for(MarkupNode *p=node->child;p;p=p->next) {
        if(!p->tag || p->attribute[9] || !strcmp(p->tag,"script") || !strcmp(p->tag,"style"))continue;
        if(index && index%columns==0)fputs("</tr><tr>",out);
        fprintf(out,"<td style=\"width:%d%%;padding:%dpx;vertical-align:top\">",100/columns,rule->gap/2);
        web_content(out,p,base,images,layout,script,1);
        fputs("</td>",out);
        index++;
    }
    fputs("</tr></tbody></table>",out);
}
static void web_content(FILE *out,MarkupNode *node,const char *base,int *images,
                        const WebLayout *layout,const WebScript *script,int single) {
    static const char *const allowed[]={"a","body","article","form","label","aside","b","blockquote","br","code","dd","div","dl","dt",
        "em","footer","h1","h2","h3","h4","h5","h6","header","hr","i","li","main","nav","ol","p","pre",
        "section","small","span","strong","sub","sup","table","tbody","td","th","thead","tr","u","ul"};
    static const char *const omit[]={"head","script","style","template","svg","math","iframe","video",
        "audio","select"};
    for(;node;node=single ? NULL : node->next) {
        if(node->text){web_escape(out,node->text);continue;}
        const char *tag=node->tag;
        if(!tag || node->attribute[9] || permitted(tag,strlen(tag),omit,sizeof(omit)/sizeof(*omit)))continue;
        if(!strcmp(tag,"img")) {
            if(!web_image(out,base,node->attribute[6],node->attribute[5],images,script ? script->network_end : 0) &&
               node->attribute[5] && *node->attribute[5]) {
                fputs("<span>[",out);web_escape(out,node->attribute[5]);fputs("]</span>",out);
            }
            continue;
        }
        int control=script_action(script,node);
        int field=script_node_id(script,node);
        if(form_editable(node) && field) {
            fputs("<a class=\"lumen-field\"",out);
            fprintf(out," href=\"lumen-field:%d\">",field);
            const char *value=node->attribute[10],*placeholder=node->attribute[ATTR_PLACEHOLDER];
            if(value && *value)web_escape(out,value);
            else if(placeholder && *placeholder)web_escape(out,placeholder);
            else if(node->attribute[ATTR_NAME] && !strcmp(node->attribute[ATTR_NAME],"q"))fputs("Search",out);
            else fputs("Enter text",out);
            fputs("</a>",out);
            continue;
        }
        if(!strcmp(tag,"input") && !form_submit_control(node))continue;
        int submit=field && form_submit_control(node) && form_owner(node);
        const char *original=tag;
        if(!strcmp(tag,"button") || submit)tag="a";
        int present=permitted(tag,strlen(tag),allowed,sizeof(allowed)/sizeof(*allowed));
        int self_closing=present && (!strcmp(tag,"br") || !strcmp(tag,"hr"));
        if(control && strcmp(tag,"a"))fprintf(out,"<a href=\"lumen-control:%d\">",control);
        if(present) {
            fprintf(out,"<%s",tag);
            if(!strcmp(tag,"a")) {
                char url[WEB_URL];
                const char *ref=node->attribute[0];
                if(submit)fprintf(out," href=\"lumen-submit:%d\"",field);
                else if(control)fprintf(out," href=\"lumen-control:%d\"",control);
                else if(url_join(base,ref,url)) {
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
            if((!strcmp(original,"button") || submit) && !node->attribute[2])fputs(" class=\"lumen-button\"",out);
            fputc('>',out);
        }
        if(submit && !form_text(node->child)) {
            const char *label=node->attribute[10] ? node->attribute[10] : node->attribute[ATTR_ARIA_LABEL];
            web_escape(out,label && *label ? label : "Search");
        } else web_children(out,node,base,images,layout,script);
        if(present && !self_closing)fprintf(out,"</%s>",tag);
        if(control && strcmp(tag,"a"))fputs("</a>",out);
    }
}
static int web_document(const char *source,const char *clean,const char *url,char title[128],const WebAction *actions,int count,
                        WebField fields[WEB_FIELDS],int *field_count,char next_url[WEB_URL]) {
    MarkupNode *root=markup_load(source,1,attributes,WEB_ATTRIBUTES);
    if(!root)return 0;
    FILE *out=fopen(clean,"wb");
    WebLayout *layout=calloc(1,sizeof(*layout));
    if(!out || !layout){if(out)fclose(out);free(layout);markup_free(root);return 0;}
    const int64_t network_end=script_time()+7500;
    WebScript *script=script_run(root,url,actions,count,network_end,next_url);
    if(script)for(int i=1;i<=script->count && *field_count<WEB_FIELDS;i++) {
        MarkupNode *node=script->nodes[i];
        if(!form_editable(node) || !script_visible(node))continue;
        WebField *field=&fields[(*field_count)++];
        field->id=i;
        const char *label=node->attribute[ATTR_PLACEHOLDER],*name=node->attribute[ATTR_NAME];
        const MarkupNode *form=form_owner(node);
        const char *method=form ? form->attribute[ATTR_METHOD] : NULL;
        const char *type=node->attribute[8],*action=form ? form->attribute[ATTR_ACTION] : NULL;
        field->submit=form && (!method || !*method || !strcasecmp(method,"get")) &&
            ((type && !strcasecmp(type,"search")) || (name && !strcmp(name,"q") && action && strstr(action,"/search")));
        if(!label || !*label)label=node->attribute[ATTR_ARIA_LABEL];
        if(!label || !*label)label=name && !strcmp(name,"q") ? "Search" : "Enter text";
        snprintf(field->label,sizeof(field->label),"%s",label);
        snprintf(field->value,sizeof(field->value),"%s",node->attribute[10] ? node->attribute[10] : "");
    }
    fputs("<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><style>"
          "@page{margin:12px 6px;}body{font-family:sans-serif;line-height:1.35;color:#111;}"
          "a{color:#222;text-decoration:underline;}pre{white-space:pre-wrap;}"
          "html,body{margin:0;}p,ul,ol{margin:0.65em 0;}h1,h2,h3{line-height:1.18;}"
          "td,th{padding:4px;} .lumen-button{background-color:#e8e8e8;padding:6px 12px;}"
          "form{margin:8px 0;padding:6px 0;}.lumen-field{display:inline-block;background-color:#f0f0f0;"
          "border:1px solid #aaaaaa;padding:8px 16px;min-width:220px;text-decoration:none;}",out);
    int requests=0;
    web_styles(out,layout,root,url,title,&requests,network_end);
    fputs("</style></head>",out);
    int images=6;
    web_content(out,root,url,&images,layout,script,0);
    fputs("</html>",out);
    int ok=!ferror(out);
    if(fclose(out))ok=0;
    script_release(script);
    free(layout);
    markup_free(root);
    return ok;
}
