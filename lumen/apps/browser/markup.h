/* Translate web documents to a static HTML subset with bounded local resources. */
enum { WEB_URL=1024, WEB_BODY=2<<20, WEB_CSS=64<<10, WEB_IMAGE=256<<10 };
static const char *const attributes[]={"href","rel","class","id","style","alt","src","onclick","type","hidden","value","media","name","action","method","placeholder","disabled","formaction","enctype","aria-label","title","oninput","onchange","onsubmit"};
#define WEB_ATTRIBUTES (sizeof(attributes)/sizeof(attributes[0]))
enum { ATTR_HREF=0, ATTR_NAME=12, ATTR_ACTION, ATTR_METHOD, ATTR_PLACEHOLDER, ATTR_DISABLED, ATTR_FORM_ACTION, ATTR_ENCTYPE, ATTR_ARIA_LABEL };
enum { WEB_EDIT, WEB_CLICK, WEB_SUBMIT, WEB_SEARCH, WEB_FIELDS=40, WEB_FIELD_TEXT=256 };
typedef struct {int id,kind;char value[WEB_FIELD_TEXT];} WebAction;
typedef struct {int id,submit;char label[80],value[WEB_FIELD_TEXT];} WebField;
static const char mobile_agent[]="Mozilla/5.0 (Linux; Kindle; Mobile) LUMEN/7.4";
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
                        const WebScript *script) {
    static const char *const allowed[]={"a","body","article","form","label","aside","b","blockquote","br","code","dd","div","dl","dt",
        "em","footer","h1","h2","h3","h4","h5","h6","header","hr","i","li","main","nav","ol","p","pre",
        "section","small","span","strong","sub","sup","table","tbody","td","th","thead","tr","u","ul"};
    static const char *const omit[]={"head","script","style","template","svg","math","iframe","video",
        "audio","select"};
    for(;node;node=node->next) {
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
            fputs("<a class=\"lumen-field",out);
            if(node->attribute[2]){fputc(' ',out);web_escape(out,node->attribute[2]);}
            fputc('"',out);
            if(node->attribute[3]){fputs(" id=\"",out);web_escape(out,node->attribute[3]);fputc('"',out);}
            fprintf(out," href=\"lumen-field:%d\">",field);
            const char *value=node->attribute[10],*placeholder=node->attribute[ATTR_PLACEHOLDER];
            if(value && *value)web_escape(out,value);
            else if(placeholder && *placeholder)web_escape(out,placeholder);
            else if(node->attribute[ATTR_ARIA_LABEL])web_escape(out,node->attribute[ATTR_ARIA_LABEL]);
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
                fputs(" style=\"",out);web_escape(out,style);fputc('"',out);
            }
            if((!strcmp(original,"button") || submit) && !node->attribute[2])fputs(" class=\"lumen-button\"",out);
            fputc('>',out);
        }
        if(submit && !form_text(node->child)) {
            const char *label=node->attribute[10] ? node->attribute[10] : node->attribute[ATTR_ARIA_LABEL];
            web_escape(out,label && *label ? label : "Go");
        } else web_content(out,node->child,base,images,script);
        if(present && !self_closing)fprintf(out,"</%s>",tag);
        if(control && strcmp(tag,"a"))fputs("</a>",out);
    }
}
static int web_document(const char *source,const char *clean,const char *url,char title[128],const WebAction *actions,int count,
                        WebField fields[WEB_FIELDS],int *field_count,char next_url[WEB_URL]) {
    MarkupNode *root=markup_load(source,1,attributes,WEB_ATTRIBUTES);
    if(!root)return 0;
    FILE *out=fopen(clean,"wb");
    if(!out){markup_free(root);return 0;}
    const int64_t network_end=script_time()+7500;
    WebScript *script=script_run(root,url,actions,count,network_end,next_url);
    if(script)for(int i=1;i<=script->count && *field_count<WEB_FIELDS;i++) {
        MarkupNode *node=script->nodes[i];
        if(!form_editable(node) || !script_visible(node))continue;
        WebField *field=&fields[(*field_count)++];
        field->id=i;
        const char *label=node->attribute[ATTR_PLACEHOLDER];
        const MarkupNode *form=form_owner(node);
        const char *method=form ? form->attribute[ATTR_METHOD] : NULL;
        const char *type=node->attribute[8];
        field->submit=form && (!method || !*method || !strcasecmp(method,"get")) &&
            ((type && !strcasecmp(type,"search")) || form_field_count(form->child)==1);
        if(!label || !*label)label=node->attribute[ATTR_ARIA_LABEL];
        if(!label || !*label)label="Enter text";
        snprintf(field->label,sizeof(field->label),"%s",label);
        snprintf(field->value,sizeof(field->value),"%s",node->attribute[10] ? node->attribute[10] : "");
    }
    fputs("<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><style>"
          "body{font-family:sans-serif;line-height:1.45;color:#161616;margin:10px;}"
          "a{color:#222;text-decoration:underline;}pre{white-space:pre-wrap;}"
          "img{max-width:100%;height:auto;}h1,h2,h3{line-height:1.2;}"
          "td,th{padding:4px;} .lumen-button{display:inline-block;background:#e8e8e8;padding:7px 12px;}"
          ".lumen-field{display:inline-block;background:#f2f2f2;border:1px solid #aaa;"
          "border-radius:5px;padding:8px 16px;min-width:150px;text-decoration:none;}</style>",out);
    web_styles(out,root,url,title);
    fputs("</head>",out);
    int images=6;
    web_content(out,root,url,&images,script);
    fputs("</html>",out);
    int ok=!ferror(out);
    if(fclose(out))ok=0;
    script_release(script);
    markup_free(root);
    return ok;
}
