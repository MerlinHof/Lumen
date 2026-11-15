/* Short article previews are served on demand without opening Wikipedia. */
#define LUMEN_JSON_IMPLEMENTATION
#include "lumen_json.h"
#include <sys/wait.h>

static int summary_fetch(const char *query,char *result,size_t capacity) {
    static const char *const languages[]={"de","en","fr","zh"};
    const char *language=languages[lumen_language>=0 && lumen_language<4 ? lumen_language : 0];
    if(!*query || strlen(query)>128)return 0;
    char encoded[385];size_t n=0;
    for(const unsigned char *at=(const unsigned char *)query;*at;at++) {
        snprintf(encoded+n,4,"%%%02X",*at);
        n+=3;
    }
    char url[900];
    snprintf(url,sizeof(url),"https://%s.wikipedia.org/w/api.php?action=query&generator=search"
        "&gsrnamespace=0&gsrlimit=1&gsrsearch=%s&prop=extracts&exintro=1"
        "&explaintext=1&exchars=570&format=json&formatversion=2",language,encoded);
    int pipefd[2];
    if(pipe(pipefd))return 0;
    pid_t child=fork();
    if(child==0) {
        close(pipefd[0]);
        if(dup2(pipefd[1],STDOUT_FILENO)<0)_exit(127);
        close(pipefd[1]);
        execlp("curl","curl","-q","--fail","--silent","--proto","=https",
            "--connect-timeout","4","--max-time","10","--max-filesize","32768",
            "--user-agent","LUMEN Kindle Reader (https://github.com/MerlinHof/Lumen)",
            "--",url,(char *)NULL);
        _exit(127);
    }
    close(pipefd[1]);
    if(child<0){close(pipefd[0]);return 0;}
    char body[32769];size_t size=0;ssize_t got=0;
    while(size<sizeof(body)-1) {
        got=read(pipefd[0],body+size,sizeof(body)-1-size);
        if(got<0 && errno==EINTR)continue;
        if(got<=0)break;
        size+=(size_t)got;
    }
    close(pipefd[0]);
    int status;pid_t waited;
    do{waited=waitpid(child,&status,0);}while(waited<0 && errno==EINTR);
    if(waited!=child || !WIFEXITED(status) || WEXITSTATUS(status) || !size || got!=0 ||
       memchr(body,0,size))return 0;
    body[size]=0;
    Json json;JsonToken tokens[512];
    if(!json_parse(&json,body,tokens,512))return 0;
    int pages=json_get(&json,json_get(&json,0,"query"),"pages");
    int first=json_at(&json,pages,0);
    char title[180],excerpt[2600];
    json_string(&json,json_get(&json,first,"title"),title,sizeof(title));
    json_string(&json,json_get(&json,first,"extract"),excerpt,sizeof(excerpt));
    if(!*title || !*excerpt || strpbrk(title,"\r\n"))return 0;
    int length=snprintf(result,capacity,"%s\n%s",title,excerpt);
    return length>0 && (size_t)length<capacity;
}
static void serve_summary(const char *operation,const char *query) {
    char data[2850]="";
    int ok=!strcmp(operation,"summary") && summary_fetch(query,data,sizeof(data));
    lumen_app_service_result(ok,data);
}
