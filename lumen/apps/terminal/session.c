#define _POSIX_C_SOURCE 200809L
#include "session.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/prctl.h>
#endif

static int64_t milliseconds(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC,&now);
    return (int64_t)now.tv_sec*1000+now.tv_nsec/1000000;
}
void terminal_session_close(TerminalSession *session) {
    if(session->pid<=0)return;
    close(session->input);
    close(session->output);
    kill(-session->pid,SIGKILL);
    while(waitpid(session->pid,NULL,0)<0 && errno==EINTR) {}
    session->pid=0;
}
static int start(TerminalSession *session,const char *directory) {
    int commands[2], output[2];
    if(pipe(commands))return 0;
    if(pipe(output)){close(commands[0]);close(commands[1]);return 0;}
    pid_t parent=getpid(), child=fork();
    if(!child) {
        setpgid(0,0);
#ifdef __linux__
        if(prctl(PR_SET_PDEATHSIG,SIGKILL) || getppid()!=parent)_exit(125);
#else
        (void)parent;
#endif
        if(chdir(directory) || dup2(commands[0],0)<0 || dup2(output[1],1)<0 ||
           dup2(output[1],2)<0)_exit(125);
        close(commands[0]);close(commands[1]);close(output[0]);close(output[1]);
        setenv("TERM","dumb",1);
        unsetenv("ENV");
        signal(SIGPIPE,SIG_DFL);
        execl("/bin/sh","sh",(char *)0);
        _exit(127);
    }
    close(commands[0]);close(output[1]);
    if(child<0){close(commands[1]);close(output[0]);return 0;}
    setpgid(child,child);
    session->pid=child;session->input=commands[1];session->output=output[0];
    fcntl(session->input,F_SETFL,O_NONBLOCK);
    fcntl(session->output,F_SETFL,O_NONBLOCK);
    return 1;
}
static int send_script(int fd,const char *command,unsigned sequence,int64_t deadline) {
    char script[2300];
    size_t n=0;
    memcpy(script,"eval '",6);n=6;
    for(const char *p=command;*p;p++) {
        if(n+100>=sizeof(script))return 0;
        if(*p=='\''){memcpy(script+n,"'\\''",4);n+=4;}
        else script[n++]=*p;
    }
    n+=(size_t)snprintf(script+n,sizeof(script)-n,
        "' </dev/null\ncommand printf '\\036LUMEN%08x:%%d\\037' \"$?\"\n",sequence);
    for(size_t sent=0;sent<n;) {
        int64_t remaining=deadline-milliseconds();
        if(remaining<=0)return 0;
        struct pollfd p={.fd=fd,.events=POLLOUT};
        if(poll(&p,1,(int)remaining)<=0){if(errno==EINTR)continue;return 0;}
        ssize_t size=write(fd,script+sent,n-sent);
        if(size>0)sent+=(size_t)size;
        else if(errno!=EINTR && errno!=EAGAIN)return 0;
    }
    return 1;
}
static void append(char *out,size_t cap,size_t *used,unsigned char c) {
    if((c==9 || c==10 || c>=32) && *used+1<cap)out[(*used)++]=(char)c;
}
int terminal_session_command(TerminalSession *session,const char *directory,
                             const char *command,char *out,size_t capacity) {
    if(!capacity)return 125;
    out[0]=0;
    signal(SIGPIPE,SIG_IGN);
    if(session->pid<=0 && !start(session,directory))return 125;
    int64_t deadline=milliseconds()+8000;
    unsigned sequence=++session->sequence;
    if(!send_script(session->input,command,sequence,deadline)){
        terminal_session_close(session);return 125;
    }
    char marker[32],status[4];
    size_t prefix=(size_t)snprintf(marker,sizeof(marker),"\036LUMEN%08x:",sequence);
    size_t match=0, digits=0, used=0;
    int result=124;
    for(;;) {
        int64_t remaining=deadline-milliseconds();
        if(remaining<=0)break;
        struct pollfd p={.fd=session->output,.events=POLLIN};
        int ready=poll(&p,1,(int)remaining);
        if(ready<0 && errno==EINTR)continue;
        if(ready<=0)break;
        unsigned char buffer[2048];
        ssize_t n=read(session->output,buffer,sizeof(buffer));
        if(n<0 && (errno==EAGAIN || errno==EINTR))continue;
        if(n<=0){result=125;break;}
        for(ssize_t i=0;i<n;i++) {
            unsigned char c=buffer[i];
            if(match==prefix && c>='0' && c<='9' && digits+1<sizeof(status)){
                status[digits++]=(char)c;continue;
            }
            if(match==prefix && c==31 && digits){
                out[used]=0;status[digits]=0;return atoi(status);
            }
            if(match<prefix && c==(unsigned char)marker[match]){match++;continue;}
            if(match) {
                for(size_t j=0;j<match;j++)append(out,capacity,&used,marker[j]);
                for(size_t j=0;j<digits;j++)append(out,capacity,&used,status[j]);
                match=digits=0;
                if(c==(unsigned char)marker[0]){match=1;continue;}
            }
            append(out,capacity,&used,c);
        }
    }
    out[used]=0;
    terminal_session_close(session);
    return result;
}
