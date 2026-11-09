#define _POSIX_C_SOURCE 200809L
#include "shell.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

int terminal_access(void) {
    struct stat gate;
    if (lstat(TERMINAL_ACCESS,&gate) || !S_ISREG(gate.st_mode)) return 0;
    time_t age=time(NULL)-gate.st_mtime;
    return age>=0 && age<TERMINAL_WINDOW;
}

static int64_t milliseconds(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC,&now);
    return (int64_t)now.tv_sec*1000+now.tv_nsec/1000000;
}

int terminal_exec(const char *command, const char *directory, char *out, size_t capacity) {
    if (!capacity) return 125;
    out[0]=0;
    int channel[2];
    if (pipe(channel)) return 125;
    pid_t pid=fork();
    if (!pid) {
        setpgid(0,0);
        int null=open("/dev/null",O_RDONLY);
        if (null<0 || chdir(directory)) _exit(125);
        dup2(null,STDIN_FILENO);
        dup2(channel[1],STDOUT_FILENO);
        dup2(channel[1],STDERR_FILENO);
        long max_fd=sysconf(_SC_OPEN_MAX);
        if (max_fd<0 || max_fd>4096) max_fd=4096;
        for (int fd=3;fd<max_fd;fd++) close(fd);
        execl("/bin/sh","sh","-c",command,(char *)0);
        _exit(127);
    }
    close(channel[1]);
    if (pid<0) { close(channel[0]); return 125; }
    setpgid(pid,pid);
    fcntl(channel[0],F_SETFL,O_NONBLOCK);
    int64_t deadline=milliseconds()+8000;
    size_t used=0;
    int status=0, ended=0, eof=0, truncated=0;
    while (milliseconds()<deadline && (!ended || !eof)) {
        struct pollfd read_end={.fd=eof ? -1 : channel[0],.events=POLLIN};
        poll(&read_end,1,50);
        if (!eof) {
            char bytes[2048];
            ssize_t n=read(channel[0],bytes,sizeof(bytes));
            if (!n) eof=1;
            else if (n>0) for (ssize_t i=0;i<n;i++) {
                unsigned char c=bytes[i];
                if (c==9 || c==10 || c>=32) {
                    if (used+1<capacity) out[used++]=(char)c;
                    else truncated=1;
                }
            }
        }
        if (!ended) ended=waitpid(pid,&status,WNOHANG)==pid;
    }
    close(channel[0]);
    if (!ended || !eof) {
        kill(-pid,SIGKILL);
        if (!ended) while (waitpid(pid,&status,0)<0 && errno==EINTR) {}
        status=124<<8;
    }
    if (truncated && used>4) {out[used-4]='\n';out[used-3]='.';out[used-2]='.';out[used-1]='.';}
    out[used]=0;
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128+WTERMSIG(status);
}
