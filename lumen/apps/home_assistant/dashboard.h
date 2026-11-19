/* Read-only sensor history; actuator actions remain in the existing input path. */
#define HA_POINTS 128
#define HA_CATEGORIES 8
typedef struct {
    char id[160];
    double value[HA_POINTS];
    time_t time[HA_POINTS];
    int count,categories;
    char category[HA_CATEGORIES][48];
} HAHistory;
static HAHistory ha_history[8],ha_preview[8];
static char ha_history_key[1500],ha_history_pending[1500];
static int ha_history_span,ha_pending_span;
static time_t ha_history_at,ha_history_retry,ha_refresh_at;
static time_t ha_timestamp(const char *s) {
    struct tm t = {0};
    int y, m, d, h, min;
    double sec;
    char separator;
    if(sscanf(s,"%d-%d-%d%c%d:%d:%lf",&y,&m,&d,&separator,&h,&min,&sec)!=7 ||
       (separator!='T' && separator!=' ') || y<1970 || y>2100 || m<1 || m>12 ||
       d<1 || d>31 || h<0 || h>23 || min<0 || min>59 || !isfinite(sec) || sec<0 || sec>=61)
        return 0;
    t.tm_year = y - 1900;
    t.tm_mon = m - 1;
    t.tm_mday = d;
    t.tm_hour = h;
    t.tm_min = min;
    t.tm_sec = (int)sec;
    time_t v = timegm(&t);
    if(strlen(s)<19)return 0;
    const char *p = s + 19;
    if (*p == '.') {
        p++;
        while (isdigit((unsigned char)*p))
            p++;
    }
    if (*p == '+' || *p == '-') {
        int oh = 0, om = 0;
        if (sscanf(p + 1, "%d:%d", &oh, &om) == 2)
            v += (*p == '+' ? -1 : 1) * (oh * 3600 + om * 60);
    }
    return v;
}
static time_t ha_sample_time(const Json *j,int object) {
    const char *const keys[]={"last_changed","last_updated","lc","lu"};
    for(size_t i=0;i<sizeof(keys)/sizeof(*keys);i++) {
        int field=json_get(j,object,keys[i]);
        if(field<0)continue;
        char stamp[80];
        json_string(j,field,stamp,sizeof(stamp));
        time_t t=ha_timestamp(stamp);
        if(t>0)return t;
        char *end;
        double number=strtod(stamp,&end);
        if(end==stamp || *end || !isfinite(number))continue;
        if(number>4102444800.0)number/=1000;
        if(number>0 && number<2147483647.0)return (time_t)number;
    }
    return 0;
}
static int ha_sample_value(HAHistory *history,const char *state,double *value) {
    if(!*state || !strcmp(state,"unknown") || !strcmp(state,"unavailable"))return 0;
    if(!strcmp(state,"on")){*value=1;return !history->categories;}
    if(!strcmp(state,"off")){*value=0;return !history->categories;}
    char *end;
    double number=strtod(state,&end);
    if(end!=state && !*end && isfinite(number)) {
        *value=number;
        return !history->categories;
    }
    if(history->count && !history->categories)return 0;
    for(int i=0;i<history->categories;i++)if(!strcmp(state,history->category[i])){
        *value=i;
        return 1;
    }
    if(history->categories==HA_CATEGORIES || strlen(state)>=sizeof(history->category[0]))return 0;
    *value=history->categories;
    snprintf(history->category[history->categories++],sizeof(history->category[0]),"%s",state);
    return 1;
}
static int ha_history_decode(const char *body,const char *requested,HAHistory out[8]) {
    size_t length=strlen(body);
    int capacity=length/4+32>700000 ? 700000 : (int)(length/4+32);
    JsonToken *tokens=malloc((size_t)capacity*sizeof(*tokens));
    Json json;
    int valid=tokens && json_parse(&json,body,tokens,capacity) &&
        (json.tokens[0].type=='[' || json.tokens[0].type=='{');
    if(valid) {
        memset(out,0,8*sizeof(*out));
        int groups=json.tokens[0].type=='[' ? json.tokens[0].count : 1;
        for(int a=0;a<groups && a<8;a++) {
            int array=json.tokens[0].type=='[' ? json_at(&json,0,a) : json_get(&json,0,requested ? requested : "");
            int first=json_at(&json,array,0);
            if(first<0)continue;
            HAHistory *history=&out[a];
            json_string(&json,json_get(&json,first,"entity_id"),history->id,sizeof(history->id));
            if(!*history->id && requested && groups==1 && !strchr(requested,','))
                snprintf(history->id,sizeof(history->id),"%.159s",requested);
            int count=json.tokens[array].count;
            int stride=count>HA_POINTS ? (count+HA_POINTS-3)/(HA_POINTS-2) : 1;
            for(int k=0,object=first;k<count && object<json.count;object=json.tokens[object].next,k++) {
                if(k%stride && k!=count-1)continue;
                int field=json_get(&json,object,"state");
                if(field<0)field=json_get(&json,object,"s");
                char state[80];
                json_string(&json,field,state,sizeof(state));
                time_t t=ha_sample_time(&json,object);
                double value;
                if(!t || history->count>=HA_POINTS || !ha_sample_value(history,state,&value))continue;
                int i=history->count++;
                while(i>0 && history->time[i-1]>t) {
                    history->time[i]=history->time[i-1];
                    history->value[i]=history->value[i-1];
                    i--;
                }
                history->time[i]=t;history->value[i]=value;
            }
        }
    }
    free(tokens);
    return valid;
}
static void ha_history_import(void) {
    char *body=ha_read("ha-history.json",8388608);
    if(body){ha_history_decode(body,NULL,ha_preview);free(body);}
}
static int ha_visible(int *out,int picking,const char *search) {
    int n = 0;
    char query[128];
    lumen_utf8_fold(search, query, sizeof(query));
    for (int i = 0; i < ha_count; i++) {
        const HAEntity *e = &ha_entities[i];
        if (!picking && ha_favorite(e->id) < 0) continue;
        if (picking && *query) {
            char name[sizeof(e->name)], id[sizeof(e->id)];
            lumen_utf8_fold(e->name, name, sizeof(name));
            lumen_utf8_fold(e->id, id, sizeof(id));
            if (!strstr(name, query) && !strstr(id, query)) continue;
        }
        out[n++] = i;
    }
    return n;
}
