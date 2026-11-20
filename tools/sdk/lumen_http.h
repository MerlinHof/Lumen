#ifndef LUMEN_HTTP_H
#define LUMEN_HTTP_H

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#ifdef __linux__
#include <sys/prctl.h>
#endif

typedef struct {
    const char *url;
    const char *bearer;
    const char *json;
    int timeout;
    int connect_timeout;
    int redirects;
    int allow_http;
    int compressed;
    int report_url;
    int redirect_target;
    const char *user_agent;
} LumenHttpRequest;

static inline int lumen_url_encode(const char *value,char *output,size_t capacity) {
    static const char hex[]="0123456789ABCDEF";
    size_t used=0;
    if(!capacity)return 0;
    output[0]=0;
    for(const unsigned char *p=(const unsigned char *)value;*p;p++) {
        int plain=(*p>='A' && *p<='Z') || (*p>='a' && *p<='z') ||
                  (*p>='0' && *p<='9') || strchr("-._~",*p);
        if(capacity-used<=(plain ? 1u : 3u)){output[0]=0;return 0;}
        if(plain)output[used++]=(char)*p;
        else {
            output[used++]='%';
            output[used++]=hex[*p>>4];
            output[used++]=hex[*p&15];
        }
    }
    output[used]=0;
    return 1;
}

static inline int lumen_http_quote(FILE *config,const char *value) {
    for(const unsigned char *p=(const unsigned char *)value;*p;p++) {
        if(*p=='"' || *p=='\\') {
            if(fputc('\\',config)==EOF)return 0;
        } else if(*p<32) {
            const char *escaped=*p=='\n' ? "\\n" : *p=='\r' ? "\\r" : *p=='\t' ? "\\t" : NULL;
            if(!escaped || fputs(escaped,config)<0)return 0;
            continue;
        }
        if(fputc(*p,config)==EOF)return 0;
    }
    return fputs("\"\n",config)>=0;
}

static inline pid_t lumen_http_start(LumenHttpRequest request,int output,size_t limit,
                                    int file_limit) {
    const char *url=request.url;
    if(!limit || !url || strpbrk(url,"\r\n\t \"\\") || request.timeout<0 || request.timeout>180 ||
       request.connect_timeout<0 || request.connect_timeout>30 ||
       request.redirects<0 || request.redirects>8 ||
       (strncmp(url,"https://",8) && (!request.allow_http || strncmp(url,"http://",7))))return -1;
    if((request.user_agent && (strlen(request.user_agent)>256 || strpbrk(request.user_agent,"\r\n"))) ||
       (request.redirect_target && (request.bearer || request.json || request.redirects || request.report_url)))
        return -1;
    int secured=request.bearer!=NULL;
    if(secured && (!*request.bearer || strpbrk(request.bearer,"\r\n\t \"\\") ||
                   request.redirects))return -1;
    FILE *config=NULL;
    if(secured || request.json) {
        config=tmpfile();
        if(!config)return -1;
        int ok=!secured || (fputs("header = \"Authorization: Bearer ",config)>=0 &&
                            lumen_http_quote(config,request.bearer));
        if(ok && request.json) {
            /* Curl interprets a leading @ in data-binary as a file reference.
               JSON requests here must start with an object or array. */
            const char *body=request.json+strspn(request.json," \t\r\n");
            ok=(*body=='{' || *body=='[') &&
               fputs("header = \"Content-Type: application/json\"\ndata-binary = \"",config)>=0 &&
               lumen_http_quote(config,body);
        }
        if(!ok || fflush(config) || fseek(config,0,SEEK_SET)){fclose(config);return -1;}
    }
    char seconds[16],connect[16],bytes[24],redirects[8];
    snprintf(seconds,sizeof(seconds),"%d",request.timeout ? request.timeout : 12);
    snprintf(connect,sizeof(connect),"%d",request.connect_timeout ? request.connect_timeout : 4);
    snprintf(bytes,sizeof(bytes),"%zu",limit);
    snprintf(redirects,sizeof(redirects),"%d",request.redirects);
    const char *protocols=request.allow_http ? "=http,https" : "=https";
    const char *args[40]={"curl","-q","--silent","--fail","--proto",protocols,
        "--proto-redir",protocols,"--connect-timeout",connect,"--max-time",seconds,
        "--max-filesize",bytes,"--globoff","--user-agent",request.user_agent ?
        request.user_agent : "LUMEN Kindle Reader (https://github.com/MerlinHof/Lumen)"};
    int count=17;
    if(config){args[count++]="--config";args[count++]="-";}
    if(request.redirects){args[count++]="--location";args[count++]="--max-redirs";args[count++]=redirects;}
    if(request.compressed)args[count++]="--compressed";
    if(request.report_url) {
        args[count++]="--write-out";
        args[count++]="\nLUMEN-EFFECTIVE-URL:%{url_effective}\n";
    }
    if(request.redirect_target) {
        args[count++]="--output";args[count++]="/dev/null";
        args[count++]="--write-out";args[count++]="%{redirect_url}";
    }
    args[count++]="--";
    args[count++]=url;
    args[count]=NULL;

#ifdef __linux__
    pid_t parent=getpid();
#endif
    pid_t child=fork();
    if(!child) {
#ifdef __linux__
        if(prctl(PR_SET_PDEATHSIG,SIGTERM) || getppid()!=parent)_exit(127);
#endif
        int input=config ? fileno(config) : open("/dev/null",O_RDONLY);
        if(input<0 || dup2(input,STDIN_FILENO)<0 || dup2(output,STDOUT_FILENO)<0)_exit(127);
        if(input>STDERR_FILENO)close(input);
        if(output>STDERR_FILENO)close(output);
        if(file_limit) {
            struct rlimit bounds={(rlim_t)limit,(rlim_t)limit};
            if(setrlimit(RLIMIT_FSIZE,&bounds))_exit(127);
        }
        execvp(args[0],(char *const *)args);
        _exit(127);
    }
    if(config)fclose(config);
    return child;
}

typedef enum {
    LUMEN_HTTP_OK, LUMEN_HTTP_INVALID, LUMEN_HTTP_TRANSPORT,
    LUMEN_HTTP_TIMEOUT, LUMEN_HTTP_REDIRECT_LIMIT, LUMEN_HTTP_TOO_LARGE,
    LUMEN_HTTP_INTERRUPTED, LUMEN_HTTP_BAD_TEXT, LUMEN_HTTP_IO
} LumenHttpResult;

static inline LumenHttpResult lumen_http_finish(pid_t child) {
    if(child<=0)return LUMEN_HTTP_INVALID;
    int status;
    pid_t waited;
    do waited=waitpid(child,&status,0);while(waited<0 && errno==EINTR);
    if(waited!=child)return LUMEN_HTTP_IO;
    if(WIFSIGNALED(status))return WTERMSIG(status)==SIGXFSZ ? LUMEN_HTTP_TOO_LARGE : LUMEN_HTTP_INTERRUPTED;
    if(!WIFEXITED(status))return LUMEN_HTTP_INTERRUPTED;
    switch(WEXITSTATUS(status)) {
    case 0:return LUMEN_HTTP_OK;
    case 18:return LUMEN_HTTP_INTERRUPTED;
    case 28:return LUMEN_HTTP_TIMEOUT;
    case 47:return LUMEN_HTTP_REDIRECT_LIMIT;
    case 63:return LUMEN_HTTP_TOO_LARGE;
    default:return LUMEN_HTTP_TRANSPORT;
    }
}
/* Blocking calls for app workers. Capacity includes the NUL terminator. */
static inline LumenHttpResult lumen_http_text_result(LumenHttpRequest request,char *body,size_t capacity) {
    if(!body || capacity<2)return LUMEN_HTTP_INVALID;
    body[0]=0;
    int channel[2];
    if(pipe(channel))return LUMEN_HTTP_IO;
    if(fcntl(channel[0],F_SETFD,FD_CLOEXEC)<0) {
        close(channel[0]);close(channel[1]);
        return LUMEN_HTTP_IO;
    }
    pid_t child=lumen_http_start(request,channel[1],capacity-1,0);
    close(channel[1]);
    size_t used=0;
    ssize_t got=0;
    if(child>0)while(used<capacity) {
        got=read(channel[0],body+used,capacity-used);
        if(got<0 && errno==EINTR)continue;
        if(got<=0)break;
        used+=(size_t)got;
    }
    close(channel[0]);
    LumenHttpResult result=lumen_http_finish(child);
    if(used>=capacity)result=LUMEN_HTTP_TOO_LARGE;
    else if(got<0)result=LUMEN_HTTP_IO;
    else if(result==LUMEN_HTTP_OK && memchr(body,0,used))result=LUMEN_HTTP_BAD_TEXT;
    body[result==LUMEN_HTTP_OK ? used : 0]=0;
    return result;
}
static inline int lumen_http_text(LumenHttpRequest request,char *body,size_t capacity) {
    return lumen_http_text_result(request,body,capacity)==LUMEN_HTTP_OK;
}

/* The caller supplies an empty private file. Partial data is discarded. */
static inline LumenHttpResult lumen_http_download_result(LumenHttpRequest request,int fd,size_t limit) {
    struct stat info;
    if(fd<0 || fstat(fd,&info) || !S_ISREG(info.st_mode) || info.st_size ||
       lseek(fd,0,SEEK_SET)!=0)return LUMEN_HTTP_INVALID;
    LumenHttpResult result=lumen_http_finish(lumen_http_start(request,fd,limit,1));
    if(result==LUMEN_HTTP_OK) {
        if(fstat(fd,&info) || info.st_size<=0 || lseek(fd,0,SEEK_SET)!=0)result=LUMEN_HTTP_IO;
        else if((uintmax_t)info.st_size>limit)result=LUMEN_HTTP_TOO_LARGE;
    }
    if(result!=LUMEN_HTTP_OK && (ftruncate(fd,0) || lseek(fd,0,SEEK_SET)!=0))return LUMEN_HTTP_IO;
    return result;
}
static inline int lumen_http_download(LumenHttpRequest request,int fd,size_t limit) {
    return lumen_http_download_result(request,fd,limit)==LUMEN_HTTP_OK;
}

/* The final URL is needed to resolve relative links after a redirect. */
static inline LumenHttpResult lumen_http_page(LumenHttpRequest request,char *body,size_t capacity,
                                             char *url,size_t url_capacity) {
    if(!url || !url_capacity)return LUMEN_HTTP_INVALID;
    *url=0;
    request.report_url=1;
    LumenHttpResult result=lumen_http_text_result(request,body,capacity);
    if(result!=LUMEN_HTTP_OK)return result;
    const char marker[]="\nLUMEN-EFFECTIVE-URL:";
    char *line=strrchr(body,'\n');
    if(!line || line==body || line[1])return LUMEN_HTTP_BAD_TEXT;
    *line=0;
    char *start=strrchr(body,'\n');
    if(!start || strncmp(start,marker,sizeof(marker)-1))return LUMEN_HTTP_BAD_TEXT;
    const char *address=start+sizeof(marker)-1;
    size_t length=strlen(address);
    if(!length || length>=url_capacity)return LUMEN_HTTP_BAD_TEXT;
    memcpy(url,address,length+1);
    *start=0;
    return LUMEN_HTTP_OK;
}
/* Resolve an HTTP-to-HTTPS upgrade without sending credentials to the
   cleartext endpoint. An explicit configured HTTP endpoint still works. */
static inline int lumen_http_same_host_tls(const char *url) {
    if(!url || strncmp(url,"http://",7))return 0;
    char target[2308];
    return lumen_http_text((LumenHttpRequest){.url=url,.timeout=3,.connect_timeout=2,
           .allow_http=1,.redirect_target=1},target,sizeof(target)) &&
        !strncmp(target,"https://",8) && !strcmp(target+8,url+7);
}
static inline int lumen_https_get(const char *url,char *body,size_t capacity) {
    return lumen_http_text((LumenHttpRequest){.url=url,.redirects=4},body,capacity) && *body;
}
static inline int lumen_http_bearer(const char *url,const char *token,const char *payload,
                                   char *body,size_t capacity,int timeout) {
    return lumen_http_text((LumenHttpRequest){.url=url,.bearer=token,.json=payload,
        .timeout=timeout,.connect_timeout=5},body,capacity);
}
#endif
