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
#ifdef __linux__
#include <signal.h>
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
    const char *args[36]={"curl","-q","--silent","--fail","--proto",protocols,
        "--proto-redir",protocols,"--connect-timeout",connect,"--max-time",seconds,
        "--max-filesize",bytes,"--user-agent","LUMEN Kindle Reader (https://github.com/MerlinHof/Lumen)"};
    int count=16;
    if(config){args[count++]="--config";args[count++]="-";}
    if(request.redirects){args[count++]="--location";args[count++]="--max-redirs";args[count++]=redirects;}
    if(request.compressed)args[count++]="--compressed";
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

static inline int lumen_http_wait(pid_t child) {
    if(child<=0)return 0;
    int status;
    pid_t waited;
    do waited=waitpid(child,&status,0);while(waited<0 && errno==EINTR);
    return waited==child && WIFEXITED(status) && WEXITSTATUS(status)==0;
}

/* Blocking calls for app workers. Capacity includes the NUL terminator. */
static inline int lumen_http_text(LumenHttpRequest request,char *body,size_t capacity) {
    if(!body || capacity<2)return 0;
    body[0]=0;
    int channel[2];
    if(pipe(channel))return 0;
    if(fcntl(channel[0],F_SETFD,FD_CLOEXEC)<0) {
        close(channel[0]);close(channel[1]);
        return 0;
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
    int ok=lumen_http_wait(child) && got==0 && used<capacity && !memchr(body,0,used);
    body[ok ? used : 0]=0;
    return ok;
}

/* The caller supplies an empty private file, and commits it only on success. */
static inline int lumen_http_download(LumenHttpRequest request,int fd,size_t limit) {
    struct stat info;
    if(fd<0 || fstat(fd,&info) || !S_ISREG(info.st_mode) || info.st_size ||
       lseek(fd,0,SEEK_SET)!=0)return 0;
    if(!lumen_http_wait(lumen_http_start(request,fd,limit,1)) ||
       fstat(fd,&info) || info.st_size<=0 || (uintmax_t)info.st_size>limit)return 0;
    return lseek(fd,0,SEEK_SET)==0;
}

static inline int lumen_https_get(const char *url,char *body,size_t capacity) {
    return lumen_http_text((LumenHttpRequest){.url=url,.redirects=4},body,capacity) && *body;
}
static inline int lumen_http_bearer(const char *url,const char *token,const char *payload,
                                   char *body,size_t capacity,int timeout) {
    return lumen_http_text((LumenHttpRequest){.url=url,.bearer=token,.json=payload,
        .timeout=timeout,.connect_timeout=5,.allow_http=1},body,capacity);
}
#endif
