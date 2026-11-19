#ifndef LUMEN_HTTP_H
#define LUMEN_HTTP_H

#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/* Use from a package worker. The caller supplies a response buffer and limit. */
static inline int lumen_https_get(const char *url,char *body,size_t capacity) {
    if(capacity<2 || strncmp(url,"https://",8) || strpbrk(url,"\r\n\t "))return 0;
    int fd[2];
    if(pipe(fd))return 0;
    char limit[24];
    snprintf(limit,sizeof(limit),"%zu",capacity-1);
    pid_t child=fork();
    if(child==0) {
        close(fd[0]);
        if(dup2(fd[1],STDOUT_FILENO)<0)_exit(127);
        close(fd[1]);
        execlp("curl","curl","-q","--silent","--fail","--location","--max-redirs","4",
               "--proto","=https","--proto-redir","=https","--connect-timeout","4",
               "--max-time","12","--max-filesize",limit,"--user-agent",
               "LUMEN Kindle Reader (https://github.com/MerlinHof/Lumen)","--",url,(char *)NULL);
        _exit(127);
    }
    close(fd[1]);
    if(child<0){close(fd[0]);return 0;}
    size_t size=0;
    ssize_t n=0;
    while(size<capacity) {
        n=read(fd[0],body+size,capacity-size);
        if(n<0 && errno==EINTR)continue;
        if(n<=0)break;
        size+=(size_t)n;
    }
    close(fd[0]);
    int status=0;
    pid_t waited;
    do {waited=waitpid(child,&status,0);} while(waited<0 && errno==EINTR);
    if(waited!=child || !WIFEXITED(status) || WEXITSTATUS(status) || n<0 ||
       !size || size>=capacity || memchr(body,0,size))return 0;
    body[size]=0;
    return 1;
}

/* Authenticated requests do not follow redirects. Call in an app worker. */
static inline int lumen_http_bearer(const char *url,const char *token,const char *payload,
                                    char *body,size_t capacity,int timeout) {
    if((strncmp(url,"https://",8) && strncmp(url,"http://",7)) ||
       strpbrk(url,"\r\n\t \"\\") || !*token || strpbrk(token,"\r\n\"\\") ||
       capacity<2 || timeout<1 || timeout>60)return 0;
    FILE *config=tmpfile();
    if(!config)return 0;
    int ok=fprintf(config,"header = \"Authorization: Bearer %s\"\n",token)>0;
    if(ok && payload) {
        ok=fputs("header = \"Content-Type: application/json\"\ndata-binary = \"",config)>=0;
        for(const char *p=payload;ok && *p;p++) {
            if((unsigned char)*p<32){ok=0;break;}
            if(*p=='"' || *p=='\\')ok=fputc('\\',config)!=EOF;
            if(ok)ok=fputc(*p,config)!=EOF;
        }
        if(ok)ok=fputs("\"\n",config)>=0;
    }
    int pipefd[2];
    if(!ok || fflush(config) || fseek(config,0,SEEK_SET) || pipe(pipefd)) {
        fclose(config);
        return 0;
    }
    char seconds[12], limit[24];
    snprintf(seconds,sizeof(seconds),"%d",timeout);
    snprintf(limit,sizeof(limit),"%zu",capacity-1);
    pid_t child=fork();
    if(!child) {
        close(pipefd[0]);
        if(dup2(fileno(config),STDIN_FILENO)<0 || dup2(pipefd[1],STDOUT_FILENO)<0)_exit(127);
        close(pipefd[1]);
        fclose(config);
        execlp("curl","curl","-q","--config","-","--silent","--fail",
               "--proto","=http,https","--connect-timeout","5","--max-time",seconds,
               "--max-filesize",limit,"--",url,(char *)NULL);
        _exit(127);
    }
    close(pipefd[1]);
    fclose(config);
    if(child<0){close(pipefd[0]);return 0;}
    ssize_t n=0;
    size_t used=0;
    while(used<capacity) {
        n=read(pipefd[0],body+used,capacity-used);
        if(n<0 && errno==EINTR)continue;
        if(n<=0)break;
        used+=(size_t)n;
    }
    close(pipefd[0]);
    int status=0;
    pid_t waited;
    do waited=waitpid(child,&status,0); while(waited<0 && errno==EINTR);
    if(waited!=child || !WIFEXITED(status) || WEXITSTATUS(status) || n<0 ||
       used>=capacity || memchr(body,0,used))return 0;
    body[used]=0;
    return 1;
}
#endif
