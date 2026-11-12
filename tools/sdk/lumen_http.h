#ifndef LUMEN_HTTP_H
#define LUMEN_HTTP_H

#include <errno.h>
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
               "--max-time","12","--max-filesize",limit,"--",url,(char *)NULL);
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

#endif
