/* Short article previews are served on demand without opening Wikipedia. */
#define LUMEN_JSON_IMPLEMENTATION
#include "lumen_json.h"
#include "lumen_http.h"

static int summary_fetch(const char *query,char *result,size_t capacity) {
    static const char *const languages[]={"de","en","fr","zh"};
    const char *language=languages[lumen_language>=0 && lumen_language<4 ? lumen_language : 0];
    if(!*query || strlen(query)>128)return 0;
    char encoded[385];
    if(!lumen_url_encode(query,encoded,sizeof(encoded)))return 0;
    char url[900];
    snprintf(url,sizeof(url),"https://%s.wikipedia.org/w/api.php?action=query&generator=search"
        "&gsrnamespace=0&gsrlimit=1&gsrsearch=%s&prop=extracts&exintro=1"
        "&explaintext=1&exchars=570&format=json&formatversion=2",language,encoded);
    char body[32769];
    if(!lumen_http_text((LumenHttpRequest){.url=url,.timeout=10},body,sizeof(body)))return 0;
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
