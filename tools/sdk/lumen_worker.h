#ifndef LUMEN_WORKER_H
#define LUMEN_WORKER_H
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/prctl.h>
#endif

/* Return -1 on failure, 0 in the worker and its PID in the app. The worker
   owns its result files and must finish with _exit(). */
static inline pid_t lumen_worker_start(void) {
#ifdef __linux__
    pid_t parent=getpid();
#endif
    pid_t child=fork();
    if(child)return child;
#ifdef __linux__
    if(prctl(PR_SET_PDEATHSIG,SIGTERM) || getppid()!=parent)_exit(127);
#endif
    int noise=open("/dev/null",O_RDWR);
    if(noise<0 || dup2(noise,STDIN_FILENO)<0 || dup2(noise,STDOUT_FILENO)<0)_exit(127);
    if(noise>STDERR_FILENO)close(noise);
    return 0;
}
/* Return 0 while running or idle, 1 on success, and -1 on failure. */
static inline int lumen_worker_poll(pid_t *worker) {
    if(*worker<=0)return 0;
    int status;
    pid_t result=waitpid(*worker,&status,WNOHANG);
    if(!result || (result<0 && errno==EINTR))return 0;
    *worker=0;
    return result>0 && WIFEXITED(status) && !WEXITSTATUS(status) ? 1 : -1;
}
static inline void lumen_worker_cancel(pid_t *worker) {
    if(*worker<=0)return;
    kill(*worker,SIGTERM);
    while(waitpid(*worker,NULL,0)<0 && errno==EINTR){}
    *worker=0;
}
#endif
