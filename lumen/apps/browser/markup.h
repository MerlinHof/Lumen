/* Translate web documents to a static HTML subset with bounded local resources. */
enum { WEB_URL=1024, WEB_BODY=3<<20, WEB_CSS=256<<10, WEB_IMAGE=256<<10 };
static const char *const attributes[]={"href","rel","class","id","style","alt","src","onclick","type","hidden","value","media","name","action","method","placeholder","disabled","formaction","http-equiv","aria-label","content","oninput","onchange","onsubmit","srcset","data-src","poster","role","dir","lang","width","height"};
#define WEB_ATTRIBUTES (sizeof(attributes)/sizeof(attributes[0]))
enum { ATTR_HREF=0, ATTR_NAME=12, ATTR_ACTION, ATTR_METHOD, ATTR_PLACEHOLDER, ATTR_DISABLED, ATTR_FORM_ACTION, ATTR_HTTP_EQUIV, ATTR_ARIA_LABEL, ATTR_CONTENT, ATTR_SRCSET=24, ATTR_LAZY_SRC, ATTR_POSTER, ATTR_ROLE, ATTR_DIR, ATTR_LANG, ATTR_WIDTH, ATTR_HEIGHT };
enum { WEB_EDIT, WEB_CLICK, WEB_SUBMIT, WEB_SEARCH, WEB_FIELDS=40, WEB_FIELD_TEXT=256,WEB_POST=8192 };
typedef struct {int id,kind;char value[WEB_FIELD_TEXT];} WebAction;
typedef struct {int id,submit;char label[80],value[WEB_FIELD_TEXT];} WebField;
static const char mobile_agent[]="Mozilla/5.0 (Linux; Kindle; Mobile) LUMEN/7.6";
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
    if(!strncmp(ref,"http://",7) || !strncmp(ref,"https://",8))size=snprintf(out,WEB_URL,"%s",ref);
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
#include "cookies.h"
#include "selector.h"
#include "script.h"

static void web_styles(FILE *out,MarkupNode *node,const char *base,char title[128]) {
    for(;node;node=node->next) {
        if(!node->tag)continue;
        if(!strcmp(node->tag,"title") && node->child && node->child->text && !*title) {
            size_t length=strlen(node->child->text);
            if(length>127)length=127;
            while(length && ((unsigned char)node->child->text[length]&192)==128)length--;
            snprintf(title,128,"%.*s",(int)length,node->child->text);
            for(char *p=title;*p;p++)if((unsigned char)*p<32)*p=' ';
        } else if(!strcmp(node->tag,"style")) {
            fputs("<style",out);
            if(node->attribute[11]) {
                fputs(" media=\"",out);web_escape(out,node->attribute[11]);fputc('"',out);
            }
            fputc('>',out);
            for(MarkupNode *child=node->child;child;child=child->next)
                if(child->text && strlen(child->text)<=WEB_CSS && !strcasestr(child->text,"</style"))
                    fputs(child->text,out);
            fputs("</style>",out);
        } else if(!strcmp(node->tag,"link") && node->attribute[1] &&
                  !strcasecmp(node->attribute[1],"stylesheet")) {
            char url[WEB_URL];
            if(web_resource_url(base,node->attribute[0],url)) {
                fputs("<link rel=\"stylesheet\" href=\"",out);web_escape(out,url);fputc('"',out);
                if(node->attribute[11]) {
                    fputs(" media=\"",out);web_escape(out,node->attribute[11]);fputc('"',out);
                }
                fputc('>',out);
            }
        }
        web_styles(out,node->child,base,title);
    }
}
static int safe_image_score(double pixels) {
    if(!(pixels>0 && pixels<=20000))return 1000000;
    int value=(int)pixels;
    return value>=WEB_VIEW_WIDTH ? value-WEB_VIEW_WIDTH : 30000+WEB_VIEW_WIDTH-value;
}
static void web_attribute(FILE *out,const char *name,const char *value) {
    if(!value)return;
    fprintf(out," %s=\"",name);web_escape(out,value);fputc('"',out);
}
static int web_image(FILE *out,const char *base,const char *src,const MarkupNode *node) {
    char url[WEB_URL];
    if(src && (!strncmp(src,"data:image/png;base64,",22) || !strncmp(src,"data:image/jpeg;base64,",23) ||
               !strncmp(src,"data:image/gif;base64,",22))) {
        if(strlen(src)>WEB_IMAGE*4/3+64)return 0;
    } else {
        if(!web_resource_url(base,src,url))return 0;
        src=url;
    }
    fputs("<img",out);
    web_attribute(out,"src",src);
    for(int i=2;i<=5;i++)web_attribute(out,attributes[i],node->attribute[i]);
    web_attribute(out,"width",node->attribute[ATTR_WIDTH]);
    web_attribute(out,"height",node->attribute[ATTR_HEIGHT]);
    fputc('>',out);
    return 1;
}
static const char *image_source(const MarkupNode *node,char chosen[WEB_URL]) {
    const char *src=node->attribute[6],*set=node->attribute[ATTR_SRCSET];
    if(set && strncmp(set,"data:",5)) {
        int score=1000000;
        for(const char *at=set;*at;) {
            at+=strspn(at," \t\r\n,");
            const char *end=at+strcspn(at," \t\r\n,");
            const char *value=end+strspn(end," \t\r\n");
            const char *comma=strchr(value,',');
            char *unit;
            double size=strtod(value,&unit);
            if(unit==value)size=WEB_VIEW_WIDTH;
            else if(*unit=='x')size*=WEB_VIEW_WIDTH;
            int quality=safe_image_score(size);
            size_t n=(size_t)(end-at);
            if(n && n<WEB_URL && quality<score) {
                score=quality;
                snprintf(chosen,WEB_URL,"%.*s",(int)n,at);
            }
            if(!comma)break;
            at=comma+1;
        }
        if(*chosen)return chosen;
    }
    if((!src || !*src || !strncmp(src,"data:",5)) && node->attribute[ATTR_LAZY_SRC])
        return node->attribute[ATTR_LAZY_SRC];
    return src;
}
static void web_content(FILE *out,MarkupNode *node,const char *base,int readable,
                        const WebScript *script) {
    static const char *const allowed[]={"a","body","article","form","label","aside","b","blockquote","br","code","dd","div","dl","dt",
        "em","footer","h1","h2","h3","h4","h5","h6","header","hr","i","li","main","nav","ol","p","pre",
        "section","small","span","strong","sub","sup","button","input","table","tbody","td","th","thead","tr","u","ul",
        "address","abbr","bdi","bdo","caption","cite","del","details","dfn","figcaption","figure","hgroup",
        "ins","kbd","mark","picture","q","ruby","rt","rp","s","samp","summary","time","var","wbr","tfoot"};
    static const char *const omit[]={"head","script","style","template","svg","math","iframe","video",
        "audio","select","noscript","object","embed","source","canvas"};
    for(;node;node=node->next) {
        if(node->text){web_escape(out,node->text);continue;}
        const char *tag=node->tag;
        if(!tag || node->attribute[9])continue;
        if(!strcmp(tag,"html")){web_content(out,node->child,base,readable,script);continue;}
        if(!strcmp(tag,"video")) {
            if(!readable)web_image(out,base,node->attribute[ATTR_POSTER],node);
            continue;
        }
        if(permitted(tag,strlen(tag),omit,sizeof(omit)/sizeof(*omit)))continue;
        if(!strcmp(tag,"img")) {
            char chosen[WEB_URL]="";
            const char *src=image_source(node,chosen);
            if((readable || !web_image(out,base,src,node)) && node->attribute[5] && *node->attribute[5]) {
                fputs("<span>[",out);web_escape(out,node->attribute[5]);fputs("]</span>",out);
            }
            continue;
        }
        int control=script_action(script,node);
        int field=script_node_id(script,node);
        if(form_editable(node) && field) {
            const char *value=node->attribute[10],*placeholder=node->attribute[ATTR_PLACEHOLDER];
            const char *label=value && *value ? value : placeholder && *placeholder ? placeholder :
                node->attribute[ATTR_ARIA_LABEL];
            if(!label || !*label)label="Enter text";
            fprintf(out,"<%s class=\"lumen-field",tag);
            if(!readable && node->attribute[2]){fputc(' ',out);web_escape(out,node->attribute[2]);}
            fprintf(out,"\" data-lumen-action=\"lumen-field:%d\"",field);
            web_attribute(out,"id",node->attribute[3]);
            web_attribute(out,"name",node->attribute[ATTR_NAME]);
            web_attribute(out,"type",node->attribute[8]);
            web_attribute(out,"role",node->attribute[ATTR_ROLE]);
            web_attribute(out,"data-lumen-label",label);
            if(!readable)web_attribute(out,"style",node->attribute[4]);
            fputc('>',out);
            if(!strcmp(tag,"textarea"))fputs("</textarea>",out);
            continue;
        }
        if(!strcmp(tag,"input") && !form_submit_control(node))continue;
        int submit=field && form_submit_control(node) && form_owner(node);
        const char *original=tag;

        int present=permitted(tag,strlen(tag),allowed,sizeof(allowed)/sizeof(*allowed));
        if(!present && !readable && strchr(tag,'-') && strlen(tag)<=80 &&
           strspn(tag,"abcdefghijklmnopqrstuvwxyz0123456789-")==strlen(tag))present=1;
        int self_closing=present && (!strcmp(tag,"br") || !strcmp(tag,"hr") || !strcmp(tag,"wbr") || !strcmp(tag,"input"));

        if(present) {
            fprintf(out,"<%s",tag);
            if(!strcmp(tag,"a")) {
                char url[WEB_URL];
                const char *ref=node->attribute[0];
                if(control)fprintf(out," href=\"lumen-control:%d\"",control);
                else if(url_join(base,ref,url)) {
                    fputs(" href=\"",out);web_escape(out,*ref=='#' ? ref : url);fputc('"',out);
                }
            } else if(submit) {
                fprintf(out," data-lumen-action=\"lumen-submit:%d\"",field);
            } else if(control && strcmp(tag,"body")) {
                fprintf(out," data-lumen-action=\"lumen-control:%d\"",control);
            }
            web_attribute(out,"id",node->attribute[3]);
            if(!readable) {
                web_attribute(out,"class",node->attribute[2]);
                web_attribute(out,"style",node->attribute[4]);
            }
            web_attribute(out,"role",node->attribute[ATTR_ROLE]);
            if(!strcmp(tag,"input") || !strcmp(tag,"button"))web_attribute(out,"type",node->attribute[8]);
            web_attribute(out,"dir",node->attribute[ATTR_DIR]);
            web_attribute(out,"lang",node->attribute[ATTR_LANG]);
            if((!strcmp(original,"button") || submit) && (readable || !node->attribute[2]))
                fputs(" class=\"lumen-button\"",out);
            if(submit && !strcmp(tag,"input")) {
                const char *label=node->attribute[10] ? node->attribute[10] : node->attribute[ATTR_ARIA_LABEL];
                web_attribute(out,"data-lumen-label",label && *label ? label : "Go");
            }
            fputc('>',out);
        }
        if(submit && !self_closing && !form_text(node->child)) {
            const char *label=node->attribute[10] ? node->attribute[10] : node->attribute[ATTR_ARIA_LABEL];
            web_escape(out,label && *label ? label : "Go");
        } else web_content(out,node->child,base,readable,script);
        if(present && !self_closing)fprintf(out,"</%s>",tag);

    }
}
static int web_meta_refresh(MarkupNode *node,const char *base,char next[WEB_URL]) {
    for(;node;node=node->next) {
        if(!node->tag || !strcmp(node->tag,"noscript") || !strcmp(node->tag,"body"))continue;
        if(!strcmp(node->tag,"meta") && node->attribute[ATTR_HTTP_EQUIV] &&
           !strcasecmp(node->attribute[ATTR_HTTP_EQUIV],"refresh") && node->attribute[ATTR_CONTENT]) {
            char *end;
            double delay=strtod(node->attribute[ATTR_CONTENT],&end);
            if(end!=node->attribute[ATTR_CONTENT] && delay>=0 && delay<=1) {
                end+=strspn(end," \t;");
                if(!strncasecmp(end,"url",3)) {
                    end+=3;end+=strspn(end," \t");
                    if(*end++=='=') {
                        end+=strspn(end," \t");
                        size_t n=strlen(end);
                        char ref[WEB_URL];
                        if(n>2 && (*end=='"' || *end=='\'') && end[n-1]==*end){end++;n-=2;}
                        if(n && n<sizeof(ref)) {
                            snprintf(ref,sizeof(ref),"%.*s",(int)n,end);
                            if(web_resource_url(base,ref,next))return 1;
                            *next=0;
                        }
                    }
                }
            }
        }
        if(web_meta_refresh(node->child,base,next))return 1;
    }
    return 0;
}
static int web_write(const char *path,MarkupNode *root,const char *url,char title[128],
                     const WebScript *script,int readable) {
    FILE *out=fopen(path,"wb");
    if(!out)return 0;
    fputs("<!DOCTYPE html><html",out);
    web_attribute(out,"lang",root->attribute[ATTR_LANG]);
    web_attribute(out,"dir",root->attribute[ATTR_DIR]);
    web_attribute(out,"id",root->attribute[3]);
    if(!readable) {
        web_attribute(out,"class",root->attribute[2]);
        web_attribute(out,"style",root->attribute[4]);
    }
    fputs("><head><meta charset=\"UTF-8\"><style>"
          "body{font-family:sans-serif;line-height:1.45;color:#161616;margin:10px;}"
          "a{color:#222;text-decoration:underline;}pre{white-space:pre-wrap;}"
          "img{max-width:100%;height:auto;}h1,h2,h3{line-height:1.2;}"
          "td,th{padding:4px;} .lumen-button{display:inline-block;background:#e8e8e8;padding:7px 12px;}"
          ".lumen-field{display:inline-block;background:#f2f2f2;border:1px solid #aaa;"
          "border-radius:5px;padding:8px 16px;min-width:150px;text-decoration:none;}"
          "[data-lumen-label]::before{content:attr(data-lumen-label);}</style>",out);
    if(!readable)web_styles(out,root,url,title);
    fputs("</head>",out);
    if(readable) {
        fputs("<h1>",out);web_escape(out,*title ? title : url);fputs("</h1>",out);
    }
    web_content(out,root,url,readable,script);
    fputs("</html>",out);
    int ok=!ferror(out);
    if(fclose(out))ok=0;
    return ok;
}
static int web_document(const char *source,const char *clean,const char *readable,const char *url,char title[128],const WebAction *actions,int count,
                        WebField fields[WEB_FIELDS],int *field_count,char next_url[WEB_URL],char post[WEB_POST]) {
    MarkupNode *root=markup_load(source,1,attributes,WEB_ATTRIBUTES);
    if(!root)return 0;
    const int64_t network_end=script_time()+7500;
    WebScript *script=script_run(root,url,actions,count,network_end,next_url,post);
    if(!*next_url && !count)web_meta_refresh(root,url,next_url);
    if(script)for(int i=1;i<=script->count && *field_count<WEB_FIELDS;i++) {
        MarkupNode *node=script->nodes[i];
        if(!form_editable(node) || !script_visible(node))continue;
        WebField *field=&fields[(*field_count)++];
        field->id=i;
        const char *label=node->attribute[ATTR_PLACEHOLDER];
        const MarkupNode *form=form_owner(node);
        const char *method=form ? form->attribute[ATTR_METHOD] : NULL;
        const char *type=node->attribute[8];
        field->submit=form && (!method || !*method || !strcasecmp(method,"get") || !strcasecmp(method,"post")) &&
            ((type && !strcasecmp(type,"search")) || form_field_count(form->child)==1);
        if(!label || !*label)label=node->attribute[ATTR_ARIA_LABEL];
        if(!label || !*label)label="Enter text";
        snprintf(field->label,sizeof(field->label),"%s",label);
        snprintf(field->value,sizeof(field->value),"%s",node->attribute[10] ? node->attribute[10] : "");
    }
    int ok=*next_url || (web_write(clean,root,url,title,script,0) &&
                         web_write(readable,root,url,title,script,1));
    script_release(script);
    markup_free(root);
    return ok;
}
