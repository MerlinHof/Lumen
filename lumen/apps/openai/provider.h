/* OpenAI requests run only in the package's on-demand worker. */
#define LUMEN_JSON_IMPLEMENTATION
#include "lumen_json.h"
#include <sys/wait.h>

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
    /* Curl reads the credentials through an anonymous stream. For POST, the
     * request body is already in package storage. */
    FILE *config=tmpfile();
    if(!config)return NULL;
    char body_path[2048],leaf[256];
    int parent=-1;
    if(!models) {
        parent=lumen_storage_parent("request.json",0,leaf);
        if(parent<0){fclose(config);return NULL;}
        const char *root=getenv("LUMEN_APP_DATA");
        int n=snprintf(body_path,sizeof(body_path),"%s/%s",root,leaf);
        close(parent);
        if(n<=0 || (size_t)n>=sizeof(body_path) || strpbrk(body_path,"\r\n\"\\")) {
            fclose(config);return NULL;
        }
    }
    fprintf(config,"header = \"Authorization: Bearer %s\"\n",key);
    if(!models)fprintf(config,"header = \"Content-Type: application/json\"\ndata-binary = \"@%s\"\n",body_path);
    if(fflush(config) || fseek(config,0,SEEK_SET)){fclose(config);return NULL;}
    int out[2];
    if(pipe(out)){fclose(config);return NULL;}
    pid_t child=fork();
    if(child==0) {
        close(out[0]);
        if(dup2(fileno(config),STDIN_FILENO)<0 || dup2(out[1],STDOUT_FILENO)<0)_exit(127);
        close(out[1]);
        execlp("curl","curl","-q","--silent","--fail","--config","-",
            "--proto","=https","--connect-timeout","8","--max-time","120",
            "--max-filesize","4194304","--",models ? "https://api.openai.com/v1/models" :
            "https://api.openai.com/v1/responses",(char *)NULL);
        _exit(127);
    }
    fclose(config);
    close(out[1]);
    if(child<0){close(out[0]);return NULL;}
    char *body=malloc(PROVIDER_LIMIT+1);
    size_t used=0;
    ssize_t n=0;
    if(body)while(used<PROVIDER_LIMIT+1) {
        n=read(out[0],body+used,PROVIDER_LIMIT+1-used);
        if(n<0 && errno==EINTR)continue;
        if(n<=0)break;
        used+=(size_t)n;
    }
    close(out[0]);
    int status;
    pid_t result;
    do {result=waitpid(child,&status,0);} while(result<0 && errno==EINTR);
    if(!body || result!=child || !WIFEXITED(status) || WEXITSTATUS(status) ||
       n<0 || !used || used>PROVIDER_LIMIT || memchr(body,0,used)){free(body);return NULL;}
    body[used]=0;
    return body;
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
