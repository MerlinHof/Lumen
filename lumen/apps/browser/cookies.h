/* Browser cookie storage uses the same Netscape-format jar as HTTP transport. */
static const char *web_cookie_jar(void) {
    static char path[4096];
    const char *root=getenv("LUMEN_APP_DATA");
    if(!root || snprintf(path,sizeof(path),"%s/browser-cookies.txt",root)>=(int)sizeof(path))return NULL;
    return path;
}
static int cookie_url(const char *url,char host[256],char path[WEB_URL]) {
    const char *scheme=strstr(url,"://");
    if(!scheme)return 0;
    const char *at=scheme+3;
    const char *port=at+strcspn(at,":/?#"),*slash=at+strcspn(at,"/?#");
    size_t length=(size_t)(port-at);
    if(!length || length>=256)return 0;
    for(size_t i=0;i<length;i++)host[i]=(char)tolower((unsigned char)at[i]);
    host[length]=0;
    if(*slash=='/')snprintf(path,WEB_URL,"%.*s",(int)strcspn(slash,"?#"),slash);
    else strcpy(path,"/");
    return 1;
}
static int cookie_fields(char *line,char *fields[7]) {
    char *at=line;
    for(int i=0;i<6;i++) {
        fields[i]=at;
        at=strchr(at,'\t');
        if(!at)return 0;
        *at++=0;
    }
    fields[6]=at;
    at[strcspn(at,"\r\n")]=0;
    return !strchr(at,'\t');
}
static int cookie_domain(const char *host,const char *domain,int subdomains) {
    if(*domain=='.')domain++;
    if(!strcasecmp(host,domain))return 1;
    size_t n=strlen(host),m=strlen(domain);
    return subdomains && m>0 && n>m && host[n-m-1]=='.' && !strcasecmp(host+n-m,domain);
}
static void web_cookie_read(const char *url,char out[4096]) {
    char host[256],path[WEB_URL],line[8192];
    *out=0;
    const char *jar=web_cookie_jar();
    if(!jar || !cookie_url(url,host,path))return;
    FILE *input=fopen(jar,"r");
    if(!input)return;
    size_t used=0;
    time_t now=time(NULL);
    while(fgets(line,sizeof(line),input)) {
        char *f[7];
        if(*line=='#' || !cookie_fields(line,f))continue;
        size_t length=strlen(f[2]);
        long long expiry=strtoll(f[4],NULL,10);
        if((expiry>0 && expiry<=now) || (!strncmp(url,"http://",7) && !strcmp(f[3],"TRUE")) ||
           !cookie_domain(host,f[0],!strcmp(f[1],"TRUE")) || strncmp(path,f[2],length) ||
           (length && f[2][length-1]!='/' && path[length] && path[length]!='/'))continue;
        int n=snprintf(out+used,4096-used,"%s%s=%s",used ? "; " : "",f[5],f[6]);
        if(n<0 || (size_t)n>=4096-used){out[used]=0;break;}
        used+=(size_t)n;
    }
    fclose(input);
}
static void web_cookie_write(const char *url,const char *text) {
    if(!*text || strlen(text)>4096 || strpbrk(text,"\r\n\t"))return;
    char host[256],path[WEB_URL];
    if(!cookie_url(url,host,path))return;
    char copy[4097];
    snprintf(copy,sizeof(copy),"%s",text);
    char *save,*pair=strtok_r(copy,";",&save);
    char *value=pair ? strchr(pair,'=') : NULL;
    if(!value || value==pair)return;
    *value++=0;
    pair+=strspn(pair," ");
    size_t end=strlen(pair);
    while(end && pair[end-1]==' ')pair[--end]=0;
    if(!end || strpbrk(pair," =,\"/"))return;
    char *last=strrchr(path,'/');
    if(last && last>path)*last=0;
    else strcpy(path,"/");
    time_t now=time(NULL);
    long long expiry=0;
    int secure=0,remove=0;
    for(char *attr=strtok_r(NULL,";",&save);attr;attr=strtok_r(NULL,";",&save)) {
        attr+=strspn(attr," ");
        if(!strcasecmp(attr,"secure"))secure=1;
        else if(!strncasecmp(attr,"domain=",7)) {
            if(!cookie_domain(host,attr+7,1))return;
        } else if(!strncasecmp(attr,"path=/",6) && strlen(attr+5)<sizeof(path))
            snprintf(path,sizeof(path),"%s",attr+5);
        else if(!strncasecmp(attr,"max-age=",8)) {
            char *end;
            long seconds=strtol(attr+8,&end,10);
            if(!*end && end!=attr+8) {
                if(seconds<=0)remove=1;
                else expiry=(long long)now+(seconds>31536000 ? 31536000 : seconds);
            }
        }
    }
    if(!strncmp(url,"http://",7) && secure)return;
    const char *jar=web_cookie_jar();
    if(!jar)return;
    FILE *input=fopen(jar,"r");
    char *encoded=NULL;
    size_t total=0;
    FILE *out=open_memstream(&encoded,&total);
    if(!out){if(input)fclose(input);return;}
    fputs("# Netscape HTTP Cookie File\n",out);
    size_t written=0;
    char line[8192],original[8192];
    while(input && fgets(line,sizeof(line),input)) {
        snprintf(original,sizeof(original),"%s",line);
        char *f[7],*data=line;
        if(!strncmp(data,"#HttpOnly_",10))data+=10;
        else if(*data=='#')continue;
        if(!cookie_fields(data,f))continue;
        if(!strcasecmp(host,f[0]) && !strcmp(path,f[2]) && !strcmp(pair,f[5])) {
            if(data!=line){remove=1;fputs(original,out);}
            continue;
        }
        long long until=strtoll(f[4],NULL,10);
        if(until && until<=now)continue;
        written+=strlen(original);
        if(written<120*1024)fputs(original,out);
    }
    if(input)fclose(input);
    if(!remove)fprintf(out,"%s\tFALSE\t%s\t%s\t%lld\t%s\t%s\n",host,path,secure ? "TRUE" : "FALSE",expiry,pair,value);
    if(!fclose(out))lumen_storage_write("browser-cookies.txt",encoded,total);
    free(encoded);
}
