static pid_t states_worker, history_worker;
static int refresh_pending;

static char *ha_read(const char *name,size_t capacity) {
    char *text=malloc(capacity+1);
    if(!text)return NULL;
    size_t length;
    if(!lumen_storage_read(name,text,capacity,&length) &&
       (lumen_storage_exists(name) || !lumen_legacy_read(name,text,capacity,&length))) {
        free(text);
        return NULL;
    }
    text[length]=0;
    return text;
}

static int ha_url_valid(const char *text) {
    if(strncmp(text,"http://",7) && strncmp(text,"https://",8))return 0;
    const char *host=strstr(text,"://")+3;
    return *host && *host!='/' && strlen(text)<=500 && !strpbrk(text,"\r\n\t\"\\@?# ");
}
static int ha_token_valid(const char *text) {
    return *text && strlen(text)<sizeof(ha_token) && !strpbrk(text,"\r\n\"\\");
}
static void ha_configure(void) {
    char text[2049],address_value[512],token_value[512];
    size_t length;
    if(lumen_storage_read("connection.json",text,sizeof(text)-1,&length)) {
        text[length]=0;
        Json json;
        JsonToken tokens[16];
        if(json_parse(&json,text,tokens,16)) {
            json_string(&json,json_get(&json,0,"url"),address_value,sizeof(address_value));
            json_string(&json,json_get(&json,0,"token"),token_value,sizeof(token_value));
            if(ha_url_valid(address_value) && ha_token_valid(token_value)) {
                snprintf(ha_url,sizeof(ha_url),"%s",address_value);
                snprintf(ha_token,sizeof(ha_token),"%s",token_value);
                return;
            }
        }
    }
    char *address=ha_read("ha-url.txt",sizeof(ha_url)-1);
    char *token=ha_read("ha-token.txt",sizeof(ha_token)-1);
    if(address && ha_url_valid(address))snprintf(ha_url,sizeof(ha_url),"%s",address);
    if(token && ha_token_valid(token))snprintf(ha_token,sizeof(ha_token),"%s",token);
    free(address);
    free(token);
}
static int ha_set_connection(const char *address,const char *token) {
    if(!*address || !*token) {
        const char *name=*address ? "ha-url.txt" : "ha-token.txt";
        const char *value=*address ? address : token;
        return lumen_storage_write(name,value,strlen(value));
    }
    char *json=NULL;
    size_t length=0;
    FILE *out=open_memstream(&json,&length);
    if(!out)return 0;
    fputs("{\"url\":",out);lumen_json_write(out,address);
    fputs(",\"token\":",out);lumen_json_write(out,token);
    fputc('}',out);
    int ok=!fclose(out) && lumen_storage_write("connection.json",json,length);
    free(json);
    return ok;
}
static int ha_endpoint(char url[2304],const char *endpoint) {
    size_t base=strlen(ha_url);
    while(base && ha_url[base-1]=='/')base--;
    char probe[540];
    int length=snprintf(probe,sizeof(probe),"%.*s/api/states",(int)base,ha_url);
    int tls=length>0 && length<(int)sizeof(probe) && lumen_http_same_host_tls(probe);
    const char *root=tls ? ha_url+7 : ha_url;
    if(tls)base-=7;
    return snprintf(url,2304,"%s%.*s%s",tls ? "https://" : "",(int)base,root,endpoint)<2304;
}
static pid_t ha_worker(const char *endpoint,const char *payload,const char *cache,size_t limit,int timeout) {
    if(!ha_url_valid(ha_url) || !ha_token_valid(ha_token))return 0;
    pid_t child=lumen_worker_start();
    if(child)return child>0 ? child : 0;
    char url[2304];
    if(!ha_endpoint(url,endpoint))_exit(1);
    char *body=malloc(limit);
    int ok=body && lumen_http_text((LumenHttpRequest){.url=url,.bearer=ha_token,.json=payload,
        .timeout=timeout,.connect_timeout=5,.allow_http=!strncmp(url,"http://",7)},body,limit);
    if(ok && cache) {
        Json json;
        int count=128000;
        JsonToken *tokens=malloc((size_t)count*sizeof(*tokens));
        ok=tokens && json_parse(&json,body,tokens,count) && json.tokens[0].type=='[' &&
           lumen_storage_write(cache,body,strlen(body));
        free(tokens);
    }
    free(body);
    _exit(ok ? 0 : 1);
}
static int ha_worker_done(pid_t *worker) {
    return lumen_worker_poll(worker);
}
