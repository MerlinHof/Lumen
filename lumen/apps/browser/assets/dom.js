((g)=>{
const lookup=g._find,get=g._get,set=g._set,make=g._make,insert=g._insert,remove=g._remove,resolve=g._resolve,active=g._active,pull=g._fetch,cookie=g._cookie,navigate=g._navigate;
const clock=g._clock, mediaMatches=g._matchMedia, cssSupports=g._cssSupports;
const nodes=new Map(), events=new Map();
const timers=new Map();
let nextTimer=1;
function listen(i,type,callback) {
    if(typeof callback!=='function')return;
    const key=i+':'+type, listeners=events.get(key)||[];
    if(listeners.length<24&&!listeners.includes(callback))listeners.push(callback);
    events.set(key,listeners);
    if(i>0&&type==='click')active(i);
}
function unlisten(i,type,callback) {
    const key=i+':'+type;
    events.set(key,(events.get(key)||[]).filter(fn=>fn!==callback));
}
const select=(s,p)=>lookup(s,p).map(wrap);
function wrap(i){if(!i)return null;if(nodes.has(i))return nodes.get(i);
const e={getAttribute:n=>get(i,n),setAttribute:(n,v)=>set(i,n,String(v)),
removeAttribute:n=>set(i,n,null),hasAttribute:n=>get(i,n)!==null,
querySelector:s=>select(s,i)[0]||null,querySelectorAll:s=>select(s,i),matches:s=>select(s,0).includes(e),
closest:s=>{for(let n=e;n;n=n.parentNode)if(n.matches(s))return n;return null;},
get children(){return select('*',i).filter(n=>n.parentNode===e);},
appendChild:n=>{if(n)insert(i,n._key,0);return n;},
insertBefore:(n,ref)=>{if(n)insert(i,n._key,ref?._key||0);return n;},
removeChild:n=>{if(n?.parentNode===e)remove(n._key);return n;},
remove:()=>remove(i),
addEventListener:(n,f)=>listen(i,n,f),removeEventListener:(n,f)=>unlisten(i,n,f),
click:()=>dispatch(i),dispatchEvent:evt=>dispatch(i,evt),
get parentNode(){return wrap(get(i,'parent'));},get parentElement(){return e.parentNode;},
get firstElementChild(){return e.children[0]||null;},
get lastElementChild(){return e.children.at(-1)||null;},
get nextElementSibling(){const a=e.parentNode?.children||[];return a[a.indexOf(e)+1]||null;},
get nodeName(){return e.tagName;},get nodeType(){return 1;},
get elements(){return e.querySelectorAll('input, textarea, button, select');},
get tagName(){return get(i,'tagName').toUpperCase();}};
Object.defineProperty(e,'_key',{value:i});
for(const n of ['textContent','innerHTML','className','id','href','src','value','hidden','checked','open','onclick','oninput','onchange','onsubmit'])
Object.defineProperty(e,n,{get:()=>n==='onclick'?null:['hidden','checked','open'].includes(n)?get(i,n)!==null:get(i,n)||'',
set:v=>{if(n.startsWith('on')&&typeof v==='function')e.addEventListener(n.slice(2),v);
else set(i,n,['hidden','checked','open'].includes(n)?v?'1':null:String(v));}});
e.classList={contains:n=>e.className.split(/\s+/).includes(n),add:(...a)=>{
e.className=[...new Set(e.className.split(/\s+/).concat(a))].filter(Boolean).join(' ');},
remove:(...a)=>{e.className=e.className.split(/\s+/).filter(n=>!a.includes(n)).join(' ');},
toggle:(n,f)=>{let on=f===undefined||f===null?!e.classList.contains(n):f;e.classList[on?'add':'remove'](n);return on;}};
e.style=new Proxy({}, {get:(_,n)=>n==='cssText'?get(i,'style')||'':'',set:(_,n,v)=>{
const k=String(n).replace(/[A-Z]/g,m=>'-'+m.toLowerCase());
set(i,'style',n==='cssText'?String(v):(get(i,'style')||'')+';'+k+':'+v);return true;}});
nodes.set(i,e);return e;}
function dispatch(i,event='click') {
    const target=wrap(i);if(!target)return true;
    const evt=typeof event==='string'?new g.Event(event,{bubbles:true,cancelable:true}):event;
    evt.target=target;
    const route=[target];
    if(evt.bubbles) {
        for(let node=target.parentNode;node;node=node.parentNode)route.push(node);
        route.push(g.document,g);
    }
    for(const node of route) {
        const id=node===g? -2 : node===g.document? -1 : node._key;
        evt.currentTarget=node;
        for(const f of events.get(id+':'+evt.type)||[]) {
            try{f.call(node,evt)}catch(_){}
            if(evt._immediate)break;
        }
        const handler=id>0?get(id,'on'+evt.type):null;
        if(handler&&!evt._immediate)try{if(Function('event',handler).call(node,evt)===false)evt.preventDefault();}catch(_){}
        if(evt._stopped)break;
    }
    return !evt.defaultPrevented;
}
g.document={get body(){return select('body',0)[0]||null;},get head(){return select('head',0)[0]||null;},get documentElement(){return select('html',0)[0]||null;},
getElementById:n=>select('#'+n,0)[0]||null,querySelector:n=>select(n,0)[0]||null,
querySelectorAll:n=>select(n,0),getElementsByClassName:n=>select('.'+n,0),getElementsByTagName:n=>select(n,0),
createElement:n=>wrap(make(String(n).toLowerCase())),readyState:'loading',
get forms(){return select('form',0);},
addEventListener:(n,f)=>listen(-1,n,f),removeEventListener:(n,f)=>unlisten(-1,n,f)};
g.window=g;g.self=g;g.innerWidth=g._viewportWidth;g.innerHeight=g._viewportHeight;
const href=g._pageUrl;const m=href.match(/^(https?:)\/\/([^/?#]+)([^?#]*)(\?[^#]*)?(#.*)?$/);
const loc=m?{protocol:m[1],host:m[2],hostname:m[2].split(':')[0],
origin:m[1]+'//'+m[2],pathname:m[3]||'/',search:m[4]||'',hash:m[5]||''}:{};
loc.assign=loc.replace=v=>navigate(String(v));loc.toString=()=>href;
Object.defineProperty(loc,'href',{get:()=>href,set:v=>navigate(String(v))});
for(const target of [g,g.document])Object.defineProperty(target,'location',{get:()=>loc,set:v=>navigate(String(v))});
g.document.URL=href;g.document.baseURI=g._baseUrl;Object.defineProperty(g.document,'cookie',{get:()=>cookie(),set:v=>cookie(String(v))});
function storage(){const data=new Map();return {get length(){return data.size;},
getItem:k=>data.get(String(k))??null,setItem:(k,v)=>{if(data.size<64)data.set(String(k),String(v));},
removeItem:k=>data.delete(String(k)),clear:()=>data.clear(),key:i=>[...data.keys()][i]??null};}
g.sessionStorage=storage();g.localStorage=storage();
g.document.hidden=false;g.removeEventListener=(n,f)=>unlisten(-2,n,f);
Object.defineProperty(g.document,'title',{get:()=>select('title',0)[0]?.textContent||'',
set:v=>{const el=select('title',0)[0];if(el)el.textContent=String(v);}});
g.navigator={userAgent:g._userAgent,language:g._language,languages:[g._language],onLine:true,maxTouchPoints:1};
const started=clock();g.performance={now:()=>clock()-started,timeOrigin:Date.now()};
g.navigator.sendBeacon=()=>false;
const digits='ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
g.btoa=v=>{const s=String(v);let out='';for(let i=0;i<s.length;i+=3){
const a=s.charCodeAt(i),b=s.charCodeAt(i+1)|0,c=s.charCodeAt(i+2)|0;
if(a>255||b>255||c>255)throw new TypeError('Invalid character');
out+=digits[a>>2]+digits[((a&3)<<4)|(b>>4)]+(i+1<s.length?digits[((b&15)<<2)|(c>>6)]:'=')+
(i+2<s.length?digits[c&63]:'=');}return out;};
g.atob=v=>{const s=String(v).replace(/\s/g,'').replace(/=+$/,'');let out='',bits=0,n=0;
for(const c of s){const x=digits.indexOf(c);if(x<0)throw new TypeError('Invalid base64');
n=(n<<6)|x;bits+=6;if(bits>=8){bits-=8;out+=String.fromCharCode((n>>bits)&255);}}return out;};
g.Event=class {
    constructor(type,opts={}) {this.type=type;this.bubbles=!!opts.bubbles;this.cancelable=!!opts.cancelable;this.defaultPrevented=false;}
    preventDefault(){if(this.cancelable)this.defaultPrevented=true;}
    stopPropagation(){this._stopped=true;}
    stopImmediatePropagation(){this._immediate=this._stopped=true;}
};
g.CustomEvent=class extends g.Event{constructor(type,opts={}){super(type,opts);this.detail=opts.detail;}};
g.addEventListener=(n,f)=>listen(-2,n,f);
g.console={log(){},warn(){},error(){}};
function response(input) {
    const parts=pull(String(input));
    if(parts===null)throw new TypeError('Network request failed');
    const [body,status,url]=parts;
    return {body,status,url,ok:status>=200&&status<300,text:async()=>body,json:async()=>JSON.parse(body)};
}
g.fetch=async(input,opts)=>{
    if(opts?.method&&String(opts.method).toUpperCase()!=='GET')throw new TypeError('Only GET requests are available');
    if(opts?.signal?.aborted)throw opts.signal.reason;
    return new Promise((resolve,reject)=>g.setTimeout(()=>{
        if(opts?.signal?.aborted){reject(opts.signal.reason);return;}
        try {
            const {body,...result}=response(input);
            resolve(result);
        } catch(error){reject(error);}
    },0));
};
g.XMLHttpRequest=class {
    readyState=0;status=0;responseText='';responseType='';
    open(method,url,async=true){this.method=String(method).toUpperCase();this.url=url;this.async=async;this._state(1);}
    _state(n){this.readyState=n;this.onreadystatechange?.();}
    send(){
        if(this.readyState!==1||this.method!=='GET')throw new TypeError('Only GET requests are available');
        const run=()=>{
            try {
                const result=response(this.url);
                this.status=result.status;this.responseURL=result.url;this.responseText=result.body;
                this.response=this.responseType==='json'?JSON.parse(result.body):result.body;
                this._state(4);this.onload?.();
            }catch(_){this._state(4);this.onerror?.();}
        };
        if(this.async)g.setTimeout(run,0);else run();
    }
};
const decode=s=>{try{return decodeURIComponent(s.replace(/\+/g,' '));}catch(_){return s;}};
const encode=s=>encodeURIComponent(s).replace(/%20/g,'+').replace(/[!'()~]/g,c=>'%'+c.charCodeAt(0).toString(16).toUpperCase());
g.URLSearchParams=class {
    constructor(init='') {
        if(typeof init==='string')this._pairs=init.replace(/^\?/,'').split('&').filter(Boolean).map(pair=>{
            const at=pair.indexOf('=');
            return at<0?[decode(pair),'']:[decode(pair.slice(0,at)),decode(pair.slice(at+1))];
        });
        else if(init?.[Symbol.iterator])this._pairs=Array.from(init,p=>[String(p[0]),String(p[1])]);
        else this._pairs=Object.entries(init).map(([n,v])=>[n,String(v)]);
    }
    append(n,v){this._pairs.push([String(n),String(v)]);}
    delete(n){this._pairs=this._pairs.filter(([key])=>key!==String(n));}
    get(n){return this._pairs.find(([key])=>key===String(n))?.[1]??null;}
    getAll(n){return this._pairs.filter(([key])=>key===String(n)).map(([,value])=>value);}
    has(n){return this.get(n)!==null;}
    set(n,v){
        n=String(n);v=String(v);
        const first=this._pairs.findIndex(([key])=>key===n);
        if(first<0)this.append(n,v);
        else this._pairs=this._pairs.filter(([key],i)=>key!==n||i===first)
            .map(([key,value],i)=>[key,i===first?v:value]);
    }
    sort(){this._pairs.sort(([a],[b])=>a<b?-1:a>b?1:0);}
    entries(){return this._pairs[Symbol.iterator]();}
    [Symbol.iterator](){return this.entries();}
    forEach(f,self){for(const [n,v] of this._pairs)f.call(self,v,n,this);}
    toString(){return this._pairs.map(([n,v])=>encode(n)+'='+encode(v)).join('&');}
};
g.URL=class {
    constructor(address,base=''){this._set(resolve(String(address)||String(base),String(base)));}
    _set(url){
        const parts=url?.match(/^(https?:)\/\/([^/?#]+)([^?#]*)(\?[^#]*)?(#.*)?$/i);
        if(!parts)throw new TypeError('Invalid URL');
        this.protocol=parts[1];this.host=parts[2];this.pathname=parts[3]||'/';this.hash=parts[5]||'';
        this.search=parts[4]||'';
    }
    get hostname(){return this.host.replace(/:\d+$/,'');}
    get origin(){return this.protocol+'//'+this.host;}
    get search(){const params=this.searchParams.toString();return params?'?'+params:'';}
    set search(value){this.searchParams=new g.URLSearchParams(String(value));}
    get href(){return this.origin+this.pathname+this.search+this.hash;}
    set href(value){this._set(resolve(String(value),this.href));}
    toString(){return this.href;}
    toJSON(){return this.href;}
};
g.TextEncoder=class {
    get encoding(){return 'utf-8';}
    encode(value=''){
        const bytes=[];
        for(const character of String(value)) {
            let cp=character.codePointAt(0);
            if(cp>=0xd800&&cp<=0xdfff)cp=0xfffd;
            if(cp<0x80)bytes.push(cp);
            else if(cp<0x800)bytes.push(0xc0|(cp>>6),0x80|(cp&63));
            else if(cp<0x10000)bytes.push(0xe0|(cp>>12),0x80|((cp>>6)&63),0x80|(cp&63));
            else bytes.push(0xf0|(cp>>18),0x80|((cp>>12)&63),0x80|((cp>>6)&63),0x80|(cp&63));
        }
        return Uint8Array.from(bytes);
    }
};
g.AbortController=class {
    constructor(){
        const listeners=new Set();
        this.signal={aborted:false,reason:undefined,onabort:null,
            addEventListener(type,fn){if(type==='abort'&&typeof fn==='function')listeners.add(fn);},
            removeEventListener(type,fn){if(type==='abort')listeners.delete(fn);},
            throwIfAborted(){if(this.aborted)throw this.reason;}};
        this._listeners=listeners;
    }
    abort(reason=new Error('The operation was aborted')){
        const signal=this.signal;
        if(signal.aborted)return;
        signal.aborted=true;signal.reason=reason;
        const event=new g.Event('abort');
        event.target=event.currentTarget=signal;
        for(const listener of this._listeners)try{listener.call(signal,event);}catch(_){}
        signal.onabort?.(event);
    }
};
function schedule(callback,delay,args,repeat) {
    if(typeof callback!=='function'||timers.size>=64)return 0;
    const ms=Math.min(2147483647,Math.max(repeat?4:0,Number(delay)||0));
    const id=nextTimer++;
    timers.set(id,{callback,args,repeat,ms,due:clock()+ms});
    return id;
}
g.setTimeout=(fn,ms,...args)=>schedule(fn,ms,args,false);
g.setInterval=(fn,ms,...args)=>schedule(fn,ms,args,true);
g.clearInterval=g.clearTimeout=id=>timers.delete(Number(id));
g.requestAnimationFrame=fn=>schedule(()=>fn(g.performance.now()),16,[],false);
g.cancelAnimationFrame=g.clearTimeout;
g.queueMicrotask=fn=>{
    if(typeof fn!=='function')throw new TypeError('Expected a callback');
    Promise.resolve().then(fn);
};
g._nextTask=()=>{
    let nextId,next;
    for(const [id,task] of timers)if(!next||task.due<next.due){nextId=id;next=task;}
    if(!next)return -1;
    const now=clock();
    if(next.due>now)return Math.max(1,Math.ceil(next.due-now));
    if(next.repeat)next.due=now+next.ms;
    else timers.delete(nextId);
    try{next.callback.apply(g,next.args);}catch(_){}
    return 0;
};
g.CSS={supports:(...args)=>args.length>1 ? cssSupports(String(args[0]),String(args[1])) : cssSupports(String(args[0]))};
g.matchMedia=query=>{
    const media=String(query);
    return {media,matches:mediaMatches(media),onchange:null,
        addListener(){},removeListener(){},addEventListener(){},removeEventListener(){}};
};
g.document.createEvent=()=>({initEvent(type,bubbles,cancelable){Object.assign(this,new g.Event(type,{bubbles,cancelable}));Object.setPrototypeOf(this,g.Event.prototype);}});
function lifecycleEvent(id,type,target) {
    const event=new g.Event(type);
    event.target=event.currentTarget=target;
    for(const listener of events.get(id+':'+type)||[])try{listener.call(target,event);}catch(_){}
    const handler=target['on'+type];
    if(typeof handler==='function')try{handler.call(target,event);}catch(_){}
}
g._lifecycle=state=>{
    if(state==='DOMContentLoaded') {
        lifecycleEvent(-1,state,document);
        lifecycleEvent(-2,state,g);
    } else {
        document.readyState=state;
        lifecycleEvent(-1,'readystatechange',document);
        if(state==='complete')lifecycleEvent(-2,'load',g);
    }
};
g._click=dispatch;
for(const n of ['_find','_get','_set','_make','_insert','_remove','_resolve','_active','_fetch','_clock','_matchMedia','_cssSupports','_pageUrl','_baseUrl','_userAgent','_language','_viewportWidth','_viewportHeight','_cookie','_navigate'])delete g[n];
})(globalThis);
