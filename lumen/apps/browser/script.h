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
    char *navigation;
    int requests,modules,cookie_writes;
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
static JSValue script_cookie(JSContext *js,JSValueConst self,int argc,JSValueConst *argv) {
    (void)self;
    WebScript *script=JS_GetContextOpaque(js);
    if(argc) {
        const char *value=JS_ToCString(js,argv[0]);
        if(value && script->cookie_writes++<24)web_cookie_write(script->base,value);
        JS_FreeCString(js,value);
        return JS_UNDEFINED;
    }
    char result[4096];
    web_cookie_read(script->base,result);
    return JS_NewString(js,result);
}
static JSValue script_navigate(JSContext *js,JSValueConst self,int argc,JSValueConst *argv) {
    (void)self;
    WebScript *script=JS_GetContextOpaque(js);
    if(!argc)return JS_UNDEFINED;
    const char *url=JS_ToCString(js,argv[0]);
    char resolved[WEB_URL];
    if(url && web_resource_url(script->base,url,resolved) && strcmp(script->base,resolved))
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
static const char dom_init[]=
"((g)=>{"
"const lookup=g._find,get=g._get,set=g._set,make=g._make,append=g._append,active=g._active,pull=g._fetch,cookie=g._cookie,navigate=g._navigate;"
"const nodes=new Map(), events=new Map(); let ready=[], timers=[];"
"const select=(s,p)=>lookup(s,p).map(wrap);"
"function wrap(i){if(!i)return null;if(nodes.has(i))return nodes.get(i);"
"const e={getAttribute:n=>get(i,n),setAttribute:(n,v)=>set(i,n,String(v)),"
"removeAttribute:n=>set(i,n,null),hasAttribute:n=>get(i,n)!==null,"
"querySelector:s=>select(s,i)[0]||null,querySelectorAll:s=>select(s,i),matches:s=>select(s,0).includes(e),"
"closest:s=>{for(let n=e;n;n=n.parentNode)if(n.matches(s))return n;return null;},"
"get children(){return select('*',i).filter(n=>n.parentNode===e);},"
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
"get forms(){return select('form',0);},"
"addEventListener:(n,f)=>{if(n==='DOMContentLoaded'&&typeof f==='function'&&ready.length<24)ready.push(f);}};"
"g.window=g;g.self=g;g.innerWidth=g._viewportWidth;g.innerHeight=g._viewportHeight;"
"const href=g._pageUrl;const m=href.match(/^(https?:)\\/\\/([^/?#]+)([^?#]*)(\\?[^#]*)?(#.*)?$/);"
"const loc=m?{protocol:m[1],host:m[2],hostname:m[2].split(':')[0],"
"origin:m[1]+'//'+m[2],pathname:m[3]||'/',search:m[4]||'',hash:m[5]||''}:{};"
"loc.assign=loc.replace=v=>navigate(String(v));loc.toString=()=>href;"
"Object.defineProperty(loc,'href',{get:()=>href,set:v=>navigate(String(v))});"
"for(const target of [g,g.document])Object.defineProperty(target,'location',{get:()=>loc,set:v=>navigate(String(v))});"
"g.document.URL=href;Object.defineProperty(g.document,'cookie',{get:()=>cookie(),set:v=>cookie(String(v))});"
"function storage(){const data=new Map();return {get length(){return data.size;},"
"getItem:k=>data.get(String(k))??null,setItem:(k,v)=>{if(data.size<64)data.set(String(k),String(v));},"
"removeItem:k=>data.delete(String(k)),clear:()=>data.clear(),key:i=>[...data.keys()][i]??null};}"
"g.sessionStorage=storage();g.localStorage=storage();"
"g.document.hidden=false;g.document.removeEventListener=()=>{};g.removeEventListener=()=>{};"
"Object.defineProperty(g.document,'title',{get:()=>select('title',0)[0]?.textContent||'',"
"set:v=>{const el=select('title',0)[0];if(el)el.textContent=String(v);}});"
"g.navigator={userAgent:g._userAgent,language:g._language,onLine:true,maxTouchPoints:1};"
"const started=Date.now();g.performance={now:()=>Date.now()-started,timeOrigin:started};"
"g.navigator.sendBeacon=()=>false;"
"const digits='ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';"
"g.btoa=v=>{const s=String(v);let out='';for(let i=0;i<s.length;i+=3){"
"const a=s.charCodeAt(i),b=s.charCodeAt(i+1)|0,c=s.charCodeAt(i+2)|0;"
"if(a>255||b>255||c>255)throw new TypeError('Invalid character');"
"out+=digits[a>>2]+digits[((a&3)<<4)|(b>>4)]+(i+1<s.length?digits[((b&15)<<2)|(c>>6)]:'=')+"
"(i+2<s.length?digits[c&63]:'=');}return out;};"
"g.atob=v=>{const s=String(v).replace(/\\s/g,'').replace(/=+$/,'');let out='',bits=0,n=0;"
"for(const c of s){const x=digits.indexOf(c);if(x<0)throw new TypeError('Invalid base64');"
"n=(n<<6)|x;bits+=6;if(bits>=8){bits-=8;out+=String.fromCharCode((n>>bits)&255);}}return out;};"
"g.Event=class{constructor(type,opts={}){this.type=type;this.bubbles=!!opts.bubbles;this.defaultPrevented=false;}"
"preventDefault(){this.defaultPrevented=true;}};"
"g.CustomEvent=class extends g.Event{constructor(type,opts={}){super(type,opts);this.detail=opts.detail;}};"
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
"g.document.createEvent=()=>({initEvent(type,bubbles,cancelable){Object.assign(this,{type,bubbles,cancelable});}});"
"g._ready=()=>{document.readyState='complete';for(const f of ready)try{f.call(document)}catch(_){};ready=[];"
"if(typeof g.onload==='function')try{g.onload()}catch(_){};};"
"g._click=dispatch;"
"for(const n of ['_find','_get','_set','_make','_append','_active','_fetch','_pageUrl','_userAgent','_language','_viewportWidth','_viewportHeight','_cookie','_navigate'])delete g[n];"
"})(globalThis);";

static void script_tree(WebScript *script,MarkupNode *node,const char *base) {
    for(;node && script->inline_count<12;node=node->next) {
        if(!node->tag)continue;
        if(!strcmp(node->tag,"script")) {
            const char *type=node->attribute[8];
            int module=type && !strcasecmp(type,"module");
            if(type && *type && !module && strcasecmp(type,"text/javascript") && strcasecmp(type,"application/javascript"))continue;
            script->inline_count++;
            if(node->attribute[6]) {
                char url[WEB_URL];
                if(script->remote_count>=3 || script_time()>=script->network_end ||
                   !web_resource_url(base,node->attribute[6],url))continue;
                script->remote_count++;
                char *body=malloc(SCRIPT_SOURCE+1);
                if(body && lumen_http_text((LumenHttpRequest){.url=url,.timeout=3,.redirects=2,.compressed=1,.user_agent=mobile_agent,.cookie_jar=web_cookie_jar(),
                        .allow_http=!strncmp(url,"http://",7)},body,SCRIPT_SOURCE+1)) {
                    script->deadline=script_time()+180;
                    script_eval(script,body,url,module ? JS_EVAL_TYPE_MODULE : JS_EVAL_TYPE_GLOBAL);
                }
                free(body);
            } else for(MarkupNode *child=node->child;child;child=child->next) {
                script->deadline=script_time()+180;
                script_eval(script,child->text,base,module ? JS_EVAL_TYPE_MODULE : JS_EVAL_TYPE_GLOBAL);
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
static int form_parameters(MarkupNode *node,const MarkupNode *submit,char *out,size_t size,int *count,int post) {
    for(;node;node=node->next) {
        if(node->tag && node->attribute[ATTR_NAME] && !node->attribute[ATTR_DISABLED]) {
            const char *type=node->attribute[8];
            if(form_editable(node) || node==submit || (!strcmp(node->tag,"input") && type && !strcasecmp(type,"hidden"))) {
                char name[256],value[4096];
                if(!lumen_url_encode(node->attribute[ATTR_NAME],name,sizeof(name)) ||
                   !lumen_url_encode(node->attribute[10] ? node->attribute[10] : "",value,sizeof(value)))return 0;
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
static WebScript *script_run(MarkupNode *root,const char *base,const WebAction *actions,int count,int64_t network_end,char next_url[WEB_URL],char post[WEB_POST]) {
    WebScript *script=calloc(1,sizeof(*script));
    if(!script)return NULL;
    script->network_end=network_end;script->base=base;script->navigation=next_url;
    script_index_tree(script,root);
    JSRuntime *runtime=JS_NewRuntime();
    if(!runtime)return script;
    JS_SetMemoryLimit(runtime,8<<20);
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
            {"_make",script_create,1},{"_append",script_append,2},{"_active",script_clickable,1},
            {"_fetch",script_fetch,1},{"_cookie",script_cookie,1},{"_navigate",script_navigate,1}
        };
        for(size_t i=0;i<sizeof(bindings)/sizeof(*bindings);i++)
            JS_SetPropertyStr(js,globals,bindings[i].name,JS_NewCFunction(js,bindings[i].call,bindings[i].name,bindings[i].args));
        JS_SetPropertyStr(js,globals,"_pageUrl",JS_NewString(js,base));
        JS_SetPropertyStr(js,globals,"_userAgent",JS_NewString(js,mobile_agent));
        const char *language=lumen_language==1 ? "en" : lumen_language==2 ? "fr" : "de";
        JS_SetPropertyStr(js,globals,"_language",JS_NewString(js,language));
        JS_SetPropertyStr(js,globals,"_viewportWidth",JS_NewInt32(js,WEB_VIEW_WIDTH));
        JS_SetPropertyStr(js,globals,"_viewportHeight",JS_NewInt32(js,WEB_VIEW_HEIGHT));
        script->deadline=script_time()+180;
        script_eval(script,dom_init,"lumen-dom",JS_EVAL_TYPE_GLOBAL);
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
                    if(form && script_event(script,click,form,"submit"))form_target(base,node,next_url,post);
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
