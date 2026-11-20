#include "quickjs.h"
#include <time.h>

enum { SCRIPT_NODES=2048, SCRIPT_SOURCE=128<<10, SCRIPT_CHANGES=128 };
typedef struct {
    JSContext *context;
    MarkupNode *nodes[SCRIPT_NODES],*detached[SCRIPT_CHANGES];
    unsigned char interactive[SCRIPT_NODES];
    int count,changes,inline_count,remote_count;
    size_t changed_bytes;
    int64_t deadline,network_end;
    const char *base;
    int requests;
} WebScript;

static int64_t script_time(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC,&now);
    return (int64_t)now.tv_sec*1000+now.tv_nsec/1000000;
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
        if(!script_visible(node) || !complex_match(node,selector,selector+length))continue;
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
static JSValue script_append(JSContext *js,JSValueConst self,int argc,JSValueConst *argv) {
    (void)self;
    WebScript *script=JS_GetContextOpaque(js);
    int parent=0,child=0;
    if(argc<2 || JS_ToInt32(js,&parent,argv[0]) || JS_ToInt32(js,&child,argv[1]) ||
       parent<1 || parent>script->count || child<1 || child>script->count)return JS_UNDEFINED;
    MarkupNode *node=script->nodes[child],*target=script->nodes[parent];
    for(MarkupNode *p=target;p;p=p->parent)if(p==node)return JS_UNDEFINED;
    for(int i=0;i<script->changes;i++)if(script->detached[i]==node && !node->parent && !node->next) {
        MarkupNode **link=&target->child;
        while(*link)link=&(*link)->next;
        *link=node;
        node->parent=target;
        script->detached[i]=NULL;
        break;
    }
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
    if(!argc || script->requests>=3 || script_time()>=script->network_end)return JS_NULL;
    const char *ref=JS_ToCString(js,argv[0]);
    char url[WEB_URL];
    int valid=ref && web_resource_url(script->base,ref,url) && same_origin(script->base,url);
    JS_FreeCString(js,ref);
    if(!valid)return JS_NULL;
    script->requests++;
    char *body=malloc(SCRIPT_SOURCE+1);
    if(!body)return JS_NULL;
    int ok=lumen_http_text((LumenHttpRequest){.url=url,.timeout=2,.connect_timeout=2,.compressed=1,
        .allow_http=!strncmp(url,"http://",7)},body,SCRIPT_SOURCE+1);
    JSValue result=ok ? JS_NewString(js,body) : JS_NULL;
    free(body);
    return result;
}
static void script_eval(WebScript *script,const char *code,const char *name) {
    if(!code || script_time()>=script->deadline)return;
    size_t length=strlen(code);
    if(length>SCRIPT_SOURCE)return;
    JSValue value=JS_Eval(script->context,code,length,name,JS_EVAL_TYPE_GLOBAL);
    if(JS_IsException(value))JS_FreeValue(script->context,JS_GetException(script->context));
    JS_FreeValue(script->context,value);
}
static const char dom_init[]=
"((g)=>{"
"const lookup=g._find,get=g._get,set=g._set,make=g._make,append=g._append,active=g._active,pull=g._fetch;"
"const nodes=new Map(), events=new Map(); let ready=[], timers=[];"
"const select=(s,p)=>lookup(s,p).map(wrap);"
"function wrap(i){if(!i)return null;if(nodes.has(i))return nodes.get(i);"
"const e={getAttribute:n=>get(i,n),setAttribute:(n,v)=>set(i,n,String(v)),"
"removeAttribute:n=>set(i,n,null),hasAttribute:n=>get(i,n)!==null,"
"querySelector:s=>select(s,i)[0]||null,querySelectorAll:s=>select(s,i),"
"appendChild:n=>{if(n)append(i,n._key);return n;},"
"addEventListener:(n,f)=>{if(['click','input','change','submit'].includes(n)&&typeof f==='function'){"
"const k=i+':'+n,a=events.get(k)||[];if(a.length<24)a.push(f);events.set(k,a);if(n==='click')active(i);}},"
"click:()=>dispatch(i),get parentNode(){return wrap(get(i,'parent'));},"
"get tagName(){return get(i,'tagName').toUpperCase();}};"
"Object.defineProperty(e,'_key',{value:i});"
"for(const n of ['textContent','innerHTML','className','id','href','src','value','hidden','onclick','oninput','onchange','onsubmit'])"
"Object.defineProperty(e,n,{get:()=>n==='onclick'?null:n==='hidden'?get(i,n)!==null:get(i,n)||'',"
"set:v=>{if(n.startsWith('on')&&typeof v==='function')e.addEventListener(n.slice(2),v);"
"else set(i,n,n==='hidden'&&!v?null:String(v));}});"
"e.classList={contains:n=>e.className.split(/\\s+/).includes(n),add:(...a)=>{"
"e.className=[...new Set(e.className.split(/\\s+/).concat(a))].filter(Boolean).join(' ');},"
"remove:(...a)=>{e.className=e.className.split(/\\s+/).filter(n=>!a.includes(n)).join(' ');},"
"toggle:(n,f)=>{let on=f===undefined||f===null?!e.classList.contains(n):f;e.classList[on?'add':'remove'](n);return on;}};"
"e.style=new Proxy({}, {get:(_,n)=>n==='cssText'?get(i,'style')||'':'',set:(_,n,v)=>{"
"const k=String(n).replace(/[A-Z]/g,m=>'-'+m.toLowerCase());"
"set(i,'style',n==='cssText'?String(v):(get(i,'style')||'')+';'+k+':'+v);return true;}});"
"nodes.set(i,e);return e;}"
"function dispatch(i,type='click'){const e=wrap(i);if(!e)return true;let cancel=false;"
"const evt={type,target:e,currentTarget:e,preventDefault(){cancel=true;}};"
"for(const f of events.get(i+':'+type)||[])try{f.call(e,evt)}catch(_){};"
"const h=get(i,'on'+type);if(h)try{if(Function('event',h).call(e,evt)===false)cancel=true;}catch(_){};"
"return !cancel;}"
"g.document={get body(){return select('body',0)[0]||null;},get documentElement(){return select('html',0)[0]||null;},"
"getElementById:n=>select('#'+n,0)[0]||null,querySelector:n=>select(n,0)[0]||null,"
"querySelectorAll:n=>select(n,0),getElementsByClassName:n=>select('.'+n,0),getElementsByTagName:n=>select(n,0),"
"createElement:n=>wrap(make(String(n).toLowerCase())),readyState:'loading',"
"addEventListener:(n,f)=>{if(n==='DOMContentLoaded'&&typeof f==='function'&&ready.length<24)ready.push(f);}};"
"g.window=g;g.self=g;g.innerWidth=544;g.innerHeight=560;"
"g.addEventListener=(n,f)=>{if(n==='load')document.addEventListener('DOMContentLoaded',f);};"
"g.console={log(){},warn(){},error(){}};"
"g.fetch=async(input,opts)=>{if(opts&&opts.method&&String(opts.method).toUpperCase()!=='GET')"
"throw new TypeError('Only GET requests are available');"
"const text=pull(String(input));if(text===null)throw new TypeError('Network request failed');"
"return {ok:true,status:200,text:async()=>text,json:async()=>JSON.parse(text)};};"
"g.setTimeout=(f,ms,...args)=>{if(typeof f!=='function'||ms>1000||timers.length>=24)return 0;"
"timers.push(()=>f(...args));return timers.length;};"
"g.clearTimeout=i=>{if(i>0&&i<=timers.length)timers[i-1]=null;};"
"g.requestAnimationFrame=f=>g.setTimeout(()=>f(0),0);"
"g._drain=()=>{const list=timers;timers=[];for(const f of list)if(f)try{f()}catch(_){};};"
"g._ready=()=>{document.readyState='complete';for(const f of ready)try{f.call(document)}catch(_){};ready=[];};"
"g._click=dispatch;"
"for(const n of ['_find','_get','_set','_make','_append','_active','_fetch'])delete g[n];"
"})(globalThis);";

static void script_tree(WebScript *script,MarkupNode *node,const char *base) {
    for(;node && script->inline_count<12;node=node->next) {
        if(!node->tag)continue;
        if(!strcmp(node->tag,"script")) {
            const char *type=node->attribute[8];
            if(type && *type && strcasecmp(type,"text/javascript") && strcasecmp(type,"application/javascript"))continue;
            script->inline_count++;
            if(node->attribute[6]) {
                char url[WEB_URL];
                if(script->remote_count>=3 || script_time()>=script->network_end ||
                   !web_resource_url(base,node->attribute[6],url))continue;
                script->remote_count++;
                char *body=malloc(SCRIPT_SOURCE+1);
                if(body && lumen_http_text((LumenHttpRequest){.url=url,.timeout=3,.redirects=2,.compressed=1,.user_agent=mobile_agent,
                        .allow_http=!strncmp(url,"http://",7)},body,SCRIPT_SOURCE+1)) {
                    script->deadline=script_time()+180;
                    script_eval(script,body,url);
                }
                free(body);
            } else for(MarkupNode *child=node->child;child;child=child->next) {
                script->deadline=script_time()+180;
                script_eval(script,child->text,"page-script");
            }
        } else script_tree(script,node->child,base);
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
static int form_parameters(MarkupNode *node,const MarkupNode *submit,char *out,size_t size,int *count) {
    for(;node;node=node->next) {
        if(node->tag && node->attribute[ATTR_NAME] && !node->attribute[ATTR_DISABLED]) {
            const char *type=node->attribute[8];
            if(form_editable(node) || node==submit || (!strcmp(node->tag,"input") && type && !strcasecmp(type,"hidden"))) {
                char name[128],value[WEB_FIELD_TEXT*3];
                if(!lumen_url_encode(node->attribute[ATTR_NAME],name,sizeof(name)) ||
                   !lumen_url_encode(node->attribute[10] ? node->attribute[10] : "",value,sizeof(value)))return 0;
                size_t used=strlen(out);
                int bytes=snprintf(out+used,size-used,"%c%s=%s",*count ? '&' : '?',name,value);
                if(bytes<0 || bytes>=(int)(size-used))return 0;
                (*count)++;
            }
        }
        if(!form_parameters(node->child,submit,out,size,count))return 0;
    }
    return 1;
}
static void form_target(const char *base,MarkupNode *submit,char out[WEB_URL]) {
    const MarkupNode *form=form_owner(submit);
    if(!form)return;
    const char *method=form->attribute[ATTR_METHOD];
    if(method && *method && strcasecmp(method,"get"))return;
    const char *action=submit->attribute[ATTR_FORM_ACTION];
    if(!action)action=form->attribute[ATTR_ACTION];
    if(action && *action) {if(!url_join(base,action,out))return;}
    else snprintf(out,WEB_URL,"%s",base);
    out[strcspn(out,"?#")]=0;
    int count=0;
    if(!form_parameters(form->child,submit,out,WEB_URL,&count))*out=0;
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
static WebScript *script_run(MarkupNode *root,const char *base,const WebAction *actions,int count,int64_t network_end,char next_url[WEB_URL]) {
    WebScript *script=calloc(1,sizeof(*script));
    if(!script)return NULL;
    script->network_end=network_end;script->base=base;
    script_index_tree(script,root);
    JSRuntime *runtime=JS_NewRuntime();
    if(!runtime)return script;
    JS_SetMemoryLimit(runtime,8<<20);
    JS_SetMaxStackSize(runtime,256<<10);
    JS_SetInterruptHandler(runtime,script_timeout,script);
    JSContext *js=JS_NewContext(runtime);
    if(js) {
        script->context=js;
        JS_SetContextOpaque(js,script);
        JSValue globals=JS_GetGlobalObject(js);
        const struct {const char *name;JSCFunction *call;int args;} bindings[]={
            {"_find",script_lookup,2},{"_get",script_get,2},{"_set",script_set,3},
            {"_make",script_create,1},{"_append",script_append,2},{"_active",script_clickable,1},
            {"_fetch",script_fetch,1}
        };
        for(size_t i=0;i<sizeof(bindings)/sizeof(*bindings);i++)
            JS_SetPropertyStr(js,globals,bindings[i].name,JS_NewCFunction(js,bindings[i].call,bindings[i].name,bindings[i].args));
        script->deadline=script_time()+180;
        script_eval(script,dom_init,"lumen-dom");
        JSValue ready=JS_GetPropertyStr(js,globals,"_ready");
        JSValue click=JS_GetPropertyStr(js,globals,"_click");
        JSValue drain=JS_GetPropertyStr(js,globals,"_drain");

        script_tree(script,root,base);
        const int64_t deadline=script_time()+350;
        script->deadline=deadline;
        JSValue result=JS_Call(js,ready,JS_UNDEFINED,0,NULL);
        if(JS_IsException(result))JS_FreeValue(js,JS_GetException(js));
        JS_FreeValue(js,result);
        for(int i=-1;i<count && script_time()<deadline;i++) {
            if(i>=0) {
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
                if(action->kind==WEB_SUBMIT || action->kind==WEB_CLICK)
                    submit=script_event(script,click,action->id,"click") && action->kind==WEB_SUBMIT;
                if(submit) {
                    int form=script_index(script,(MarkupNode *)form_owner(node));
                    if(form && script_event(script,click,form,"submit"))form_target(base,node,next_url);
                }
            }
            for(int pass=0;pass<2 && script_time()<deadline;pass++) {
                result=JS_Call(js,drain,JS_UNDEFINED,0,NULL);
                if(JS_IsException(result))JS_FreeValue(js,JS_GetException(js));
                JS_FreeValue(js,result);
                for(int j=0;j<64 && JS_IsJobPending(runtime) && script_time()<deadline;j++) {
                    JSContext *job;
                    if(JS_ExecutePendingJob(runtime,&job)<0){JS_FreeValue(job,JS_GetException(job));break;}
                }
            }
        }
        JS_FreeValue(js,drain);JS_FreeValue(js,ready);JS_FreeValue(js,click);JS_FreeValue(js,globals);
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
