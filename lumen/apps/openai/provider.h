/* OpenAI requests run only in the package's on-demand worker. */
#define LUMEN_JSON_IMPLEMENTATION
#include "lumen_json.h"
#include "lumen_http.h"

enum { PROVIDER_LIMIT=4194304, REPLY_LIMIT=131072 };

static char *provider_read(const char *name,size_t capacity,int legacy) {
    char *text=malloc(capacity+1);
    if(!text)return NULL;
    size_t length;
    int ok=legacy ? lumen_legacy_read(name,text,capacity,&length) :
        lumen_storage_read(name,text,capacity,&length);
    if(!ok || memchr(text,0,length)){free(text);return NULL;}
    text[length]=0;
    return text;
}

static int provider_key_valid(const char *key) {
    return key && *key && strlen(key)<=2048 && !strpbrk(key,"\r\n\t \"\\");
}

static char *provider_key(void) {
    char *key=provider_read("llm-key.txt",2048,1);
    if(!key)key=provider_read("api-key.txt",2048,0);
    if(provider_key_valid(key))return key;
    free(key);
    return NULL;
}

static int provider_save_key(const char *key) {
    const char *root=getenv("LUMEN_LEGACY_DATA");
    if(root && *root)return lumen_storage_write_at(root,"llm-key.txt",key,strlen(key));
    return lumen_storage_write("api-key.txt",key,strlen(key));
}

static int model_compare(const void *a,const void *b) {
    return strcmp(a,b);
}

static int parse_models(const char *response,char ids[512][160]) {
    JsonToken *tokens=calloc(50000,sizeof(*tokens));
    Json json;
    int count=0;
    if(tokens && json_parse(&json,response,tokens,50000)) {
        int array=json_get(&json,0,"data");
        for(int i=0;i<512;i++) {
            int item=json_at(&json,array,i);
            if(item<0)break;
            json_string(&json,json_get(&json,item,"id"),ids[count],160);
            if(*ids[count])count++;
        }
    }
    free(tokens);
    qsort(ids,count,sizeof(*ids),model_compare);
    return count;
}

static char *provider_request(const char *key,int models) {
    char *payload=models ? NULL : provider_read("request.json",PROVIDER_LIMIT,0);
    if(!models && !payload)return NULL;
    char *body=malloc(PROVIDER_LIMIT+1);
    LumenHttpRequest request={
        .url=models ? "https://api.openai.com/v1/models" : "https://api.openai.com/v1/responses",
        .bearer=key,.json=payload,.timeout=120,.connect_timeout=8
    };
    int ok=body && lumen_http_text(request,body,PROVIDER_LIMIT+1) && *body;
    free(payload);
    if(ok)return body;
    free(body);
    return NULL;
}

static int store_reply(const char *id,const char *response) {
    JsonToken *tokens=calloc(40000,sizeof(*tokens));
    char *reply=malloc(REPLY_LIMIT+1);
    if(!tokens || !reply){free(tokens);free(reply);return 0;}
    size_t length=0;
    Json json;
    int valid=json_parse(&json,response,tokens,40000);
    if(valid) {
        int output=json_get(&json,0,"output");
        for(int i=0,item;(item=json_at(&json,output,i))>=0;i++) {
            int content=json_get(&json,item,"content");
            for(int j=0,part;(part=json_at(&json,content,j))>=0;j++) {
                char type[24];
                json_string(&json,json_get(&json,part,"type"),type,sizeof(type));
                if(strcmp(type,"output_text"))continue;
                int token=json_get(&json,part,"text");
                if(token<0 || json.tokens[token].type!='"' || length==REPLY_LIMIT)continue;
                WireField field={.value=response+json.tokens[token].start,
                                 .end=response+json.tokens[token].end};
                int clipped=0;
                if(wire_decode_string(field.value,reply+length,REPLY_LIMIT-length,&clipped)!=field.end ||
                   clipped){valid=0;break;}
                length+=strlen(reply+length);
                reply[length++]='\n';
            }
            if(!valid)break;
        }
    }
    char name[48];snprintf(name,sizeof(name),"reply-%s.txt",id);
    int ok=valid && length && lumen_storage_write(name,reply,length);
    free(tokens);free(reply);
    return ok;
}

static void provider_serve(const char *operation,const char *data) {
    int models=!strcmp(operation,"models_refresh");
    int valid=models ? !*data : !strcmp(operation,"send") &&
        strlen(data)==16 && strspn(data,"0123456789abcdef")==16;
    char *key=valid ? provider_key() : NULL;
    char *response=key ? provider_request(key,models) : NULL;
    if(key){memset(key,0,strlen(key));free(key);}
    int ok=0;
    if(response) {
        if(models) {
            char (*ids)[160]=calloc(512,sizeof(*ids));
            ok=ids && parse_models(response,ids)>0 &&
                lumen_storage_write("models.json",response,strlen(response));
            free(ids);
        } else ok=store_reply(data,response);
    }
    free(response);
    lumen_app_service_result(ok,"");
}
