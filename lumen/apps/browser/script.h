#include "quickjs.h"
#include <time.h>

enum { SCRIPT_NODES=4096, SCRIPT_SOURCE=384<<10, SCRIPT_CHANGES=128 };
typedef struct {
    JSContext *context;
    MarkupNode *nodes[SCRIPT_NODES],*detached[SCRIPT_CHANGES];
    unsigned char interactive[SCRIPT_NODES];
    int count,changes,remote_count;
    size_t changed_bytes;
    int64_t deadline,network_end;
    const char *url,*base;
    char *navigation;
    int requests,modules,cookie_writes;
} WebScript;

static int64_t script_time(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC,&now);
    return (int64_t)now.tv_sec*1000+now.tv_nsec/1000000;
}
static JSValue script_clock(JSContext *js,JSValueConst self,int argc,JSValueConst *argv) {
    (void)self;(void)argc;(void)argv;
    return JS_NewInt64(js,script_time());
}
static JSValue script_match_media(JSContext *js,JSValueConst self,int argc,JSValueConst *argv) {
    (void)self;
    const char *query=argc ? JS_ToCString(js,argv[0]) : NULL;
    int match=web_engine_media_matches(query,WEB_VIEW_WIDTH,WEB_VIEW_HEIGHT);
    JS_FreeCString(js,query);
    return JS_NewBool(js,match);
}
static JSValue script_css_supports(JSContext *js,JSValueConst self,int argc,JSValueConst *argv) {
    (void)self;
    const char *property=argc ? JS_ToCString(js,argv[0]) : NULL;
    const char *value=argc>1 ? JS_ToCString(js,argv[1]) : NULL;
    int supported=web_engine_css_supports(property,value);
    JS_FreeCString(js,value);JS_FreeCString(js,property);
    return JS_NewBool(js,supported);
}
static int script_timeout(JSRuntime *runtime,void *opaque) {
    (void)runtime;
    return script_time()>=((WebScript *)opaque)->deadline;
}
static int script_index(WebScript *script,MarkupNode *node) {
    if(!node)return 0;
    for(int i=1;i<=script->count;i++)if(script->nodes[i]==node)return i;
    if(script->count+1==SCRIPT_NODES || !node->tag)return 0;
    script->nodes[++script->count]=node;
    if(node->attribute[7])script->interactive[script->count]=1;
    return script->count;
}
static void script_index_tree(WebScript *script,MarkupNode *node) {
    for(;node && script->count+1<SCRIPT_NODES;node=node->next) {
        script_index(script,node);
        script_index_tree(script,node->child);
    }
}
static int script_visible(MarkupNode *node) {
    for(;node;node=node->parent)
        if(!node->parent && (!node->tag || strcmp(node->tag,"html")))return 0;
    return 1;
}
static inline JSValue script_lookup(JSContext *js,JSValueConst self,int argc,JSValueConst *argv) {
    (void)self;
    WebScript *script=JS_GetContextOpaque(js);
    JSValue result=JS_NewArray(js);
    if(!argc)return result;
    const char *selector=JS_ToCString(js,argv[0]);
    if(!selector)return result;
    size_t length=strlen(selector);
    int count=0,ancestor=0;
    if(argc>1)JS_ToInt32(js,&ancestor,argv[1]);
    if(ancestor<0 || ancestor>script->count)ancestor=0;
    for(int i=1;length<=95 && i<=script->count;i++) {
        MarkupNode *node=script->nodes[i];
        if((!ancestor && !script_visible(node)) || !complex_match(node,selector,selector+length))continue;
        if(ancestor) {
            MarkupNode *p=node->parent;
            while(p && p!=script->nodes[ancestor])p=p->parent;
            if(!p)continue;
        }
        JS_SetPropertyUint32(js,result,count++,JS_NewInt32(js,i));
    }
    JS_FreeCString(js,selector);
    return result;
}
static int script_field(const char *name) {
    if(!strcmp(name,"className"))return 2;
    for(size_t i=0;i<WEB_ATTRIBUTES;i++)if(!strcmp(attributes[i],name))return (int)i;
    return -1;
}
static void script_flat(FILE *out,MarkupNode *node) {
    for(;node;node=node->next) {
        if(node->text)fputs(node->text,out);
        script_flat(out,node->child);
    }
}
static JSValue script_get(JSContext *js,JSValueConst self,int argc,JSValueConst *argv) {
    (void)self;
    WebScript *script=JS_GetContextOpaque(js);
    int id=0;
    if(argc<2 || JS_ToInt32(js,&id,argv[0]) || id<1 || id>script->count)return JS_NULL;
    const char *key=JS_ToCString(js,argv[1]);
    if(!key)return JS_NULL;
    MarkupNode *node=script->nodes[id];
    JSValue value=JS_NULL;
    int field=script_field(key);
    if(field>=0 && node->attribute[field])value=JS_NewString(js,node->attribute[field]);
    else if(!strcmp(key,"tagName"))value=JS_NewString(js,node->tag);
    else if(!strcmp(key,"parent"))value=JS_NewInt32(js,script_index(script,node->parent));
    else if(!strcmp(key,"textContent") || !strcmp(key,"innerHTML")) {
        char *text=NULL;size_t length=0;
        FILE *out=open_memstream(&text,&length);
        if(out) {
            script_flat(out,node->child);
            if(!fclose(out) && length<=SCRIPT_SOURCE)value=JS_NewStringLen(js,text,length);
            free(text);
        }
    }
    JS_FreeCString(js,key);
    return value;
}
static MarkupNode *script_fragment(const char *html,size_t length) {
    const char *root=getenv("LUMEN_APP_DATA");
    const char name[]="browser-script.tmp";
    char path[4096];
    if(!root || snprintf(path,sizeof(path),"%s/%s",root,name)>=(int)sizeof(path) ||
       !lumen_storage_write(name,html,length))return NULL;
    MarkupNode *tree=markup_load(path,1,attributes,WEB_ATTRIBUTES);
    lumen_storage_delete(name);
    MarkupNode *body=tree;
    if(body && body->tag && !strcmp(body->tag,"html"))body=body->child;
    while(body && (!body->tag || strcmp(body->tag,"body")))body=body->next;
    MarkupNode *content=body ? body->child : NULL;
    if(body)body->child=NULL;
    markup_free(tree);
    return content;
}
static JSValue script_set(JSContext *js,JSValueConst self,int argc,JSValueConst *argv) {
    (void)self;
    WebScript *script=JS_GetContextOpaque(js);
    int id=0;
    if(argc<3 || JS_ToInt32(js,&id,argv[0]) || id<1 || id>script->count)return JS_UNDEFINED;
    if(script->changes==SCRIPT_CHANGES)return JS_UNDEFINED;
    const char *key=JS_ToCString(js,argv[1]);
    size_t length=0;
    const char *value=JS_IsNull(argv[2]) ? NULL : JS_ToCStringLen(js,&length,argv[2]);
    MarkupNode *node=script->nodes[id];
    if(key && length<=SCRIPT_SOURCE && script->changed_bytes+length<1<<20) {
        script->changed_bytes+=length;
        int field=script_field(key);
        if(field>=0 && length<=8192) {
            char *copy=value ? strdup(value) : NULL;
            free(node->attribute[field]);node->attribute[field]=copy;
        } else if(value && (!strcmp(key,"textContent") || !strcmp(key,"innerHTML"))) {
            MarkupNode *replacement=NULL;
            if(!strcmp(key,"innerHTML"))replacement=script_fragment(value,length);
            else {
                replacement=calloc(1,sizeof(*replacement));
                if(replacement)replacement->text=strdup(value);
            }
            if(node->child) {
                script->detached[script->changes++]=node->child;
                for(MarkupNode *p=node->child;p;p=p->next)p->parent=NULL;
            }
            node->child=replacement;
            for(MarkupNode *p=replacement;p;p=p->next)p->parent=node;
            script_index_tree(script,replacement);
        }
    }
    JS_FreeCString(js,key);
    JS_FreeCString(js,value);
    return JS_UNDEFINED;
}
static JSValue script_create(JSContext *js,JSValueConst self,int argc,JSValueConst *argv) {
    (void)self;
    WebScript *script=JS_GetContextOpaque(js);
    if(!argc || script->changes==SCRIPT_CHANGES || script->count+1==SCRIPT_NODES)return JS_NULL;
    const char *tag=JS_ToCString(js,argv[0]);
    if(!tag)return JS_NULL;
    size_t length=strlen(tag);
    MarkupNode *node=NULL;
    if(length && length<=20 && strspn(tag,"abcdefghijklmnopqrstuvwxyz0123456789")==length) {
        node=calloc(1,sizeof(*node));
        if(node && !(node->tag=strdup(tag))){free(node);node=NULL;}
    }
    JS_FreeCString(js,tag);
    if(!node)return JS_NULL;
    script->detached[script->changes++]=node;
    return JS_NewInt32(js,script_index(script,node));
}
static JSValue script_insert(JSContext *js,JSValueConst self,int argc,JSValueConst *argv) {
    (void)self;
    WebScript *script=JS_GetContextOpaque(js);
    int parent=0,child=0;
    if(argc<2 || JS_ToInt32(js,&parent,argv[0]) || JS_ToInt32(js,&child,argv[1]) ||
       parent<1 || parent>script->count || child<1 || child>script->count)return JS_UNDEFINED;
    MarkupNode *node=script->nodes[child],*target=script->nodes[parent];
    for(MarkupNode *p=target;p;p=p->parent)if(p==node)return JS_UNDEFINED;
    int before=0;
    if(argc>2)JS_ToInt32(js,&before,argv[2]);
    if(before<0 || before>script->count || before==child)return JS_UNDEFINED;
    MarkupNode *reference=before ? script->nodes[before] : NULL;
    if(reference && reference->parent!=target)return JS_UNDEFINED;
    if(node->parent) {
        MarkupNode **old=&node->parent->child;
        while(*old!=node)old=&(*old)->next;
        *old=node->next;
    } else {
        int found=0;
        for(int i=0;i<script->changes;i++)if(script->detached[i]==node && !node->next) {
            script->detached[i]=NULL;
            found=1;
            break;
        }
        if(!found)return JS_UNDEFINED;
    }
    MarkupNode **link=&target->child;
    while(*link && *link!=reference)link=&(*link)->next;
    node->next=*link;
    *link=node;
    node->parent=target;
    return JS_UNDEFINED;
}
static JSValue script_remove(JSContext *js,JSValueConst self,int argc,JSValueConst *argv) {
    (void)self;
    WebScript *script=JS_GetContextOpaque(js);
    int id=0;
    if(!argc || JS_ToInt32(js,&id,argv[0]) || id<1 || id>script->count ||
       script->changes==SCRIPT_CHANGES)return JS_UNDEFINED;
    MarkupNode *node=script->nodes[id];
    if(!node->parent)return JS_UNDEFINED;
    MarkupNode **link=&node->parent->child;
    while(*link!=node)link=&(*link)->next;
    *link=node->next;
    node->parent=node->next=NULL;
    script->detached[script->changes++]=node;
    return JS_UNDEFINED;
}
static JSValue script_clickable(JSContext *js,JSValueConst self,int argc,JSValueConst *argv) {
    (void)self;
    WebScript *script=JS_GetContextOpaque(js);
    int id=0;
    if(argc && !JS_ToInt32(js,&id,argv[0]) && id>=1 && id<=script->count)script->interactive[id]=1;
    return JS_UNDEFINED;
}
/* Scripts can read small same-origin responses. No OS, socket or module API
   is installed in the QuickJS context. */
static JSValue script_fetch(JSContext *js,JSValueConst self,int argc,JSValueConst *argv) {
    (void)self;
    WebScript *script=JS_GetContextOpaque(js);
    if(!argc || script->requests>=6 || script_time()>=script->network_end)return JS_NULL;
    const char *ref=JS_ToCString(js,argv[0]);
    char url[WEB_URL];
    int valid=ref && web_resource_url(script->base,ref,url) && same_origin(script->url,url);
    JS_FreeCString(js,ref);
    if(!valid)return JS_NULL;
    script->requests++;
    char *body=malloc(SCRIPT_SOURCE+1);
    if(!body)return JS_NULL;
    char final[WEB_URL];
    int status;
    int ok=lumen_http_page_status((LumenHttpRequest){.url=url,.timeout=2,.connect_timeout=2,.compressed=1,
        .user_agent=mobile_agent,.cookie_jar=web_cookie_jar(),.allow_http=!strncmp(url,"http://",7)},
        body,SCRIPT_SOURCE+1,final,sizeof(final),&status)==LUMEN_HTTP_OK;
    JSValue result=ok ? JS_NewArray(js) : JS_NULL;
    if(ok) {
        JS_SetPropertyUint32(js,result,0,JS_NewString(js,body));
        JS_SetPropertyUint32(js,result,1,JS_NewInt32(js,status));
        JS_SetPropertyUint32(js,result,2,JS_NewString(js,final));
    }
    free(body);
    return result;
}
static JSValue script_cookie(JSContext *js,JSValueConst self,int argc,JSValueConst *argv) {
    (void)self;
    WebScript *script=JS_GetContextOpaque(js);
    if(argc) {
        const char *value=JS_ToCString(js,argv[0]);
        if(value && script->cookie_writes++<24)web_cookie_write(script->url,value);
        JS_FreeCString(js,value);
        return JS_UNDEFINED;
    }
    char result[4096];
    web_cookie_read(script->url,result);
    return JS_NewString(js,result);
}
static JSValue script_resolve(JSContext *js,JSValueConst self,int argc,JSValueConst *argv) {
    (void)self;
    if(argc<2)return JS_NULL;
    const char *ref=JS_ToCString(js,argv[0]),*base=JS_ToCString(js,argv[1]);
    char url[WEB_URL];
    int valid=ref && base && (url_valid(ref) || (url_valid(base) && url_join(base,ref,url)));
    JSValue result=valid ? JS_NewString(js,url_valid(ref) ? ref : url) : JS_NULL;
    JS_FreeCString(js,ref);JS_FreeCString(js,base);
    return result;
}
static JSValue script_navigate(JSContext *js,JSValueConst self,int argc,JSValueConst *argv) {
    (void)self;
    WebScript *script=JS_GetContextOpaque(js);
    if(!argc)return JS_UNDEFINED;
    const char *url=JS_ToCString(js,argv[0]);
    char resolved[WEB_URL];
    if(url && web_resource_url(script->url,url,resolved) && strcmp(script->url,resolved))
        snprintf(script->navigation,WEB_URL,"%s",resolved);
    JS_FreeCString(js,url);
    return JS_UNDEFINED;
}
static char *script_module_name(JSContext *js,const char *parent,const char *name,void *opaque) {
    WebScript *script=opaque;
    char url[WEB_URL];
    if(!strstr(name,"://") && *name!='.' && *name!='/') {
        JS_ThrowReferenceError(js,"Unsupported module specifier");
        return NULL;
    }
    if(!web_resource_url(url_valid(parent) ? parent : script->base,name,url)) {
        JS_ThrowReferenceError(js,"Unsupported module address");
        return NULL;
    }
    size_t length=strlen(url)+1;
    char *copy=js_malloc(js,length);
    if(copy)memcpy(copy,url,length);
    return copy;
}
static JSModuleDef *script_module_load(JSContext *js,const char *url,void *opaque) {
    WebScript *script=opaque;
    if(!url_valid(url) || script->modules>=8 || script_time()>=script->network_end) {
        JS_ThrowReferenceError(js,"Module is unavailable");
        return NULL;
    }
    script->modules++;
    char *body=malloc(SCRIPT_SOURCE+1);
    int ok=body && lumen_http_text((LumenHttpRequest){.url=url,.timeout=3,.redirects=2,.compressed=1,
         .user_agent=mobile_agent,.cookie_jar=web_cookie_jar(),.allow_http=!strncmp(url,"http://",7)},body,SCRIPT_SOURCE+1);
    if(!ok) {
        free(body);JS_ThrowReferenceError(js,"Unable to load module");return NULL;
    }
    JSValue result=JS_Eval(js,body,strlen(body),url,JS_EVAL_TYPE_MODULE|JS_EVAL_FLAG_COMPILE_ONLY);
    free(body);
    if(JS_IsException(result))return NULL;
    JSModuleDef *module=JS_VALUE_GET_PTR(result);
    JSValue meta=JS_GetImportMeta(js,module);
    JS_SetPropertyStr(js,meta,"url",JS_NewString(js,url));
    JS_FreeValue(js,meta);
    JS_FreeValue(js,result);
    return module;
}
static void script_eval(WebScript *script,const char *code,const char *name,int flags) {
    if(!code || script_time()>=script->deadline)return;
    size_t length=strlen(code);
    if(length>SCRIPT_SOURCE)return;
    JSValue value=JS_Eval(script->context,code,length,name,flags);
    if(JS_IsException(value))JS_FreeValue(script->context,JS_GetException(script->context));
    JS_FreeValue(script->context,value);
}
static char *script_bootstrap(void) {
    const char *assets=getenv("LUMEN_APP_ASSETS");
    char path[4096];
    if(!assets || snprintf(path,sizeof(path),"%s/assets/dom.js",assets)>=(int)sizeof(path))return NULL;
    FILE *file=fopen(path,"rb");
    if(!file)return NULL;
    char *code=malloc(SCRIPT_SOURCE+1);
    size_t length=code ? fread(code,1,SCRIPT_SOURCE,file) : 0;
    int ok=length && length<SCRIPT_SOURCE && !ferror(file);
    fclose(file);
    if(!ok){free(code);return NULL;}
    code[length]=0;
    return code;
}

enum { SCRIPT_TAGS=48 };
typedef struct {MarkupNode *node;int phase,module;} ScriptTag;

static void script_collect(MarkupNode *node,ScriptTag tags[SCRIPT_TAGS],int *count) {
    for(;node && *count<SCRIPT_TAGS;node=node->next) {
        if(!node->tag || !strcmp(node->tag,"template") || !strcmp(node->tag,"noscript"))continue;
        if(strcmp(node->tag,"script")) {
            script_collect(node->child,tags,count);
            continue;
        }
        const char *type=node->attribute[8];
        int module=type && !strcasecmp(type,"module");
        if(node->attribute[ATTR_NO_MODULE] ||
           (type && *type && !module && strcasecmp(type,"text/javascript") &&
            strcasecmp(type,"application/javascript")))continue;
        int external=node->attribute[6]!=NULL;
        int phase=external && node->attribute[ATTR_ASYNC] ? 2 :
                  module || (external && node->attribute[ATTR_DEFER]) ? 1 : 0;
        tags[(*count)++]=(ScriptTag){node,phase,module};
    }
}
static void script_phase(WebScript *script,const ScriptTag *tags,int count,int phase) {
    if(*script->navigation)return;
    for(int i=0;i<count && !*script->navigation && script_time()<script->network_end;i++) {
        if(tags[i].phase!=phase)continue;
        const MarkupNode *node=tags[i].node;
        int flags=tags[i].module ? JS_EVAL_TYPE_MODULE : JS_EVAL_TYPE_GLOBAL;
        if(node->attribute[6]) {
            char url[WEB_URL],final[WEB_URL];
            if(script->remote_count>=10 || !web_resource_url(script->base,node->attribute[6],url))continue;
            script->remote_count++;
            char *body=malloc(SCRIPT_SOURCE+WEB_URL+48);
            if(body && lumen_http_page((LumenHttpRequest){.url=url,.timeout=3,.redirects=2,.compressed=1,
                    .user_agent=mobile_agent,.cookie_jar=web_cookie_jar(),.allow_http=!strncmp(url,"http://",7)},
                    body,SCRIPT_SOURCE+WEB_URL+48,final,sizeof(final))==LUMEN_HTTP_OK && url_valid(final)) {
                script->deadline=script_time()+220;
                script_eval(script,body,final,flags);
            }
            free(body);
        } else for(const MarkupNode *child=node->child;child;child=child->next) {
            script->deadline=script_time()+220;
            script_eval(script,child->text,script->base,flags);
        }
    }
}
static void script_jobs(WebScript *script,JSRuntime *runtime) {
    for(int jobs=0;jobs<96 && JS_IsJobPending(runtime) && script_time()<script->deadline;jobs++) {
        JSContext *job=NULL;
        if(JS_ExecutePendingJob(runtime,&job)<0 && job)JS_FreeValue(job,JS_GetException(job));
    }
}
static void script_lifecycle(WebScript *script,JSValue lifecycle,const char *state) {
    JSContext *js=script->context;
    JSValue arg=JS_NewString(js,state);
    script->deadline=script_time()+220;
    JSValue result=JS_Call(js,lifecycle,JS_UNDEFINED,1,&arg);
    if(JS_IsException(result))JS_FreeValue(js,JS_GetException(js));
    JS_FreeValue(js,result);JS_FreeValue(js,arg);
}
static void script_pump(WebScript *script,JSRuntime *runtime,JSValue next_task,int window_ms) {
    JSContext *js=script->context;
    const int64_t stop=script_time()+window_ms;
    for(int turns=0;turns<128 && !*script->navigation && script_time()<=stop;turns++) {
        script->deadline=script_time()+160;
        script_jobs(script,runtime);
        if(*script->navigation)break;
        JSValue result=JS_Call(js,next_task,JS_UNDEFINED,0,NULL);
        int32_t delay=-1;
        if(JS_IsException(result))JS_FreeValue(js,JS_GetException(js));
        else if(JS_IsNumber(result))JS_ToInt32(js,&delay,result);
        JS_FreeValue(js,result);
        if(delay<0)break;
        if(delay>0) {
            if(delay>stop-script_time())break;
            struct timespec pause={delay/1000,(delay%1000)*1000000L};
            nanosleep(&pause,NULL);
        }
    }
}
static int form_editable(const MarkupNode *node) {
    if(!node || !node->tag || node->attribute[9] || node->attribute[ATTR_DISABLED])return 0;
    if(!strcmp(node->tag,"textarea"))return 1;
    const char *type=node->attribute[8];
    return !strcmp(node->tag,"input") && (!type || !*type || !strcasecmp(type,"text") ||
        !strcasecmp(type,"search") || !strcasecmp(type,"email") || !strcasecmp(type,"url") ||
        !strcasecmp(type,"tel") || !strcasecmp(type,"number"));
}
static int form_field_count(const MarkupNode *node) {
    int count=0;
    for(;node && count<2;node=node->next) {
        count+=form_editable(node);
        count+=form_field_count(node->child);
    }
    return count;
}
static int form_text(const MarkupNode *node) {
    for(;node;node=node->next) {
        if(node->text && node->text[strspn(node->text," \r\n\t")])return 1;
        if(node->tag && !strcmp(node->tag,"svg"))continue;
        if(form_text(node->child))return 1;
    }
    return 0;
}
static int form_submit_control(const MarkupNode *node) {
    const char *type=node->attribute[8];
    return (!strcmp(node->tag,"button") && (!type || !*type || !strcasecmp(type,"submit"))) ||
        (!strcmp(node->tag,"input") && type && !strcasecmp(type,"submit"));
}
static const MarkupNode *form_owner(const MarkupNode *node) {
    for(node=node->parent;node;node=node->parent)
        if(node->tag && !strcmp(node->tag,"form"))return node;
    return NULL;
}
static int form_toggle(const MarkupNode *node) {
    const char *type=node->attribute[8];
    return node->tag && !strcmp(node->tag,"input") && type &&
        (!strcasecmp(type,"checkbox") || !strcasecmp(type,"radio"));
}
static void set_boolean(MarkupNode *node,int field,int enabled) {
    free(node->attribute[field]);
    node->attribute[field]=enabled ? strdup("1") : NULL;
}
static int web_click_default(WebScript *script,MarkupNode *node) {
    if(form_toggle(node) && !node->attribute[ATTR_DISABLED]) {
        const char *name=node->attribute[ATTR_NAME];
        int radio=!strcasecmp(node->attribute[8],"radio");
        if(radio && node->attribute[ATTR_CHECKED])return 0;
        if(radio && name)for(int i=1;i<=script->count;i++) {
            MarkupNode *other=script->nodes[i];
            if(other!=node && form_toggle(other) && !strcasecmp(other->attribute[8],"radio") &&
               other->attribute[ATTR_NAME] && !strcmp(other->attribute[ATTR_NAME],name) &&
               form_owner(other)==form_owner(node))set_boolean(other,ATTR_CHECKED,0);
        }
        set_boolean(node,ATTR_CHECKED,!node->attribute[ATTR_CHECKED]);
        return 1;
    }
    if(!strcmp(node->tag,"summary") && node->parent && node->parent->tag &&
       !strcmp(node->parent->tag,"details"))
        set_boolean(node->parent,ATTR_OPEN,!node->parent->attribute[ATTR_OPEN]);
    return 0;
}
static int form_parameters(MarkupNode *node,const MarkupNode *submit,char *out,size_t size,int *count,int post) {
    for(;node;node=node->next) {
        if(node->tag && node->attribute[ATTR_NAME] && !node->attribute[ATTR_DISABLED]) {
            const char *type=node->attribute[8];
            if(form_editable(node) || node==submit || (form_toggle(node) && node->attribute[ATTR_CHECKED]) || (!strcmp(node->tag,"input") && type && !strcasecmp(type,"hidden"))) {
                char name[256],value[4096];
                if(!lumen_url_encode(node->attribute[ATTR_NAME],name,sizeof(name)) ||
                   !lumen_url_encode(node->attribute[10] ? node->attribute[10] : form_toggle(node) ? "on" : "",value,sizeof(value)))return 0;
                size_t used=strlen(out);
                const char *separator=*count ? "&" : post ? "" : "?";
                int bytes=snprintf(out+used,size-used,"%s%s=%s",separator,name,value);
                if(bytes<0 || bytes>=(int)(size-used))return 0;
                (*count)++;
            }
        }
        if(!form_parameters(node->child,submit,out,size,count,post))return 0;
    }
    return 1;
}
static void form_target(const char *base,MarkupNode *submit,char out[WEB_URL],char body[WEB_POST]) {
    const MarkupNode *form=form_owner(submit);
    if(!form)return;
    const char *method=form->attribute[ATTR_METHOD];
    int post=method && !strcasecmp(method,"post");
    if(method && *method && !post && strcasecmp(method,"get"))return;
    const char *action=submit->attribute[ATTR_FORM_ACTION];
    if(!action)action=form->attribute[ATTR_ACTION];
    if(action && *action) {if(!url_join(base,action,out))return;}
    else snprintf(out,WEB_URL,"%s",base);
    out[strcspn(out,post ? "#" : "?#")]=0;
    int count=0;
    *body=0;
    if(!form_parameters(form->child,submit,post ? body : out,post ? WEB_POST : WEB_URL,&count,post))
        *out=*body=0;
}
static int script_event(WebScript *script,JSValue dispatch,int id,const char *event) {
    JSContext *js=script->context;
    JSValue args[]={JS_NewInt32(js,id),JS_NewString(js,event)};
    JSValue result=JS_Call(js,dispatch,JS_UNDEFINED,2,args);
    int allowed=!JS_IsException(result) && JS_ToBool(js,result)>0;
    if(JS_IsException(result))JS_FreeValue(js,JS_GetException(js));
    JS_FreeValue(js,args[1]);JS_FreeValue(js,result);
    return allowed;
}
static WebScript *script_run(MarkupNode *root,const char *url,const char *base,const WebAction *actions,int count,int64_t network_end,char next_url[WEB_URL],char post[WEB_POST]) {
    WebScript *script=calloc(1,sizeof(*script));
    if(!script)return NULL;
    script->network_end=network_end;script->url=url;script->base=base;script->navigation=next_url;
    script_index_tree(script,root);
    JSRuntime *runtime=JS_NewRuntime();
    if(!runtime)return script;
    JS_SetMemoryLimit(runtime,12<<20);
    JS_SetMaxStackSize(runtime,256<<10);
    JS_SetInterruptHandler(runtime,script_timeout,script);
    JS_SetModuleLoaderFunc(runtime,script_module_name,script_module_load,script);
    JSContext *js=JS_NewContext(runtime);
    if(js) {
        script->context=js;
        JS_SetContextOpaque(js,script);
        JSValue globals=JS_GetGlobalObject(js);
        const struct {const char *name;JSCFunction *call;int args;} bindings[]={
            {"_find",script_lookup,2},{"_get",script_get,2},{"_set",script_set,3},
            {"_make",script_create,1},{"_insert",script_insert,3},{"_remove",script_remove,1},{"_resolve",script_resolve,2},{"_active",script_clickable,1},
            {"_clock",script_clock,0},{"_matchMedia",script_match_media,1},{"_cssSupports",script_css_supports,2},{"_fetch",script_fetch,1},{"_cookie",script_cookie,1},{"_navigate",script_navigate,1}
        };
        for(size_t i=0;i<sizeof(bindings)/sizeof(*bindings);i++)
            JS_SetPropertyStr(js,globals,bindings[i].name,JS_NewCFunction(js,bindings[i].call,bindings[i].name,bindings[i].args));
        JS_SetPropertyStr(js,globals,"_pageUrl",JS_NewString(js,url));
        JS_SetPropertyStr(js,globals,"_baseUrl",JS_NewString(js,base));
        JS_SetPropertyStr(js,globals,"_userAgent",JS_NewString(js,mobile_agent));
        const char *language=lumen_language==1 ? "en" : lumen_language==2 ? "fr" : "de";
        JS_SetPropertyStr(js,globals,"_language",JS_NewString(js,language));
        JS_SetPropertyStr(js,globals,"_viewportWidth",JS_NewInt32(js,WEB_VIEW_WIDTH));
        JS_SetPropertyStr(js,globals,"_viewportHeight",JS_NewInt32(js,WEB_VIEW_HEIGHT));
        script->deadline=script_time()+180;
        char *bootstrap=script_bootstrap();
        script_eval(script,bootstrap,"lumen-dom",JS_EVAL_TYPE_GLOBAL);
        free(bootstrap);
        JSValue lifecycle=JS_GetPropertyStr(js,globals,"_lifecycle");
        JSValue click=JS_GetPropertyStr(js,globals,"_click");
        JSValue task=JS_GetPropertyStr(js,globals,"_nextTask");

        ScriptTag tags[SCRIPT_TAGS];
        int tag_count=0;
        script_collect(root,tags,&tag_count);
        script_phase(script,tags,tag_count,0);
        script_lifecycle(script,lifecycle,"interactive");
        script_phase(script,tags,tag_count,1);
        script->deadline=script_time()+220;
        script_jobs(script,runtime);
        script_lifecycle(script,lifecycle,"DOMContentLoaded");
        script_phase(script,tags,tag_count,2);
        script_lifecycle(script,lifecycle,"complete");
        script_pump(script,runtime,task,1600);
        const int64_t replay_end=script_time()+2000;
        for(int i=0;i<count && !*next_url && script_time()<replay_end;i++) {
            script->deadline=script_time()+220;
            const WebAction *action=&actions[i];
            if(action->id<1 || action->id>script->count)continue;
            MarkupNode *node=script->nodes[action->id];
            if((action->kind==WEB_EDIT || action->kind==WEB_SEARCH) && form_editable(node)) {
                char *copy=strdup(action->value);
                if(copy){free(node->attribute[10]);node->attribute[10]=copy;}
                script_event(script,click,action->id,"input");
                script_event(script,click,action->id,"change");
            }
            int submit=action->kind==WEB_SEARCH;
            if(action->kind==WEB_SUBMIT || action->kind==WEB_CLICK) {
                int allowed=script_event(script,click,action->id,"click");
                submit=allowed && action->kind==WEB_SUBMIT;
                if(allowed && action->kind==WEB_CLICK && web_click_default(script,node)) {
                    script_event(script,click,action->id,"input");
                    script_event(script,click,action->id,"change");
                }
            }
            if(submit) {
                int form=script_index(script,(MarkupNode *)form_owner(node));
                if(form && script_event(script,click,form,"submit"))form_target(base,node,next_url,post);
            }
            script_pump(script,runtime,task,i==count-1 ? 650 : 150);
        }
        JS_FreeValue(js,task);JS_FreeValue(js,lifecycle);JS_FreeValue(js,click);JS_FreeValue(js,globals);
        JS_FreeContext(js);
        script->context=NULL;
    }
    JS_FreeRuntime(runtime);
    return script;
}
static int script_node_id(const WebScript *script,const MarkupNode *node) {
    if(script)for(int i=1;i<=script->count;i++)if(script->nodes[i]==node)return i;
    return 0;
}
static int script_action(const WebScript *script,const MarkupNode *node) {
    if(script)for(int i=1;i<=script->count;i++)
        if(script->interactive[i] && script->nodes[i]==node)return i;
    return 0;
}
static void script_release(WebScript *script) {
    if(!script)return;
    for(int i=0;i<script->changes;i++)markup_free(script->detached[i]);
    free(script);
}
