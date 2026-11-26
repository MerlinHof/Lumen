#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
static struct {
    int enabled,scanning;
    char ssid[128];
} wifi_ui;

static struct {
    pid_t child;
    int fd,state;
    char target[129],reason[192];
} wifi_join={.fd=-1};

static void wifi_join_poll(void) {
    if(wifi_join.child<=0)return;
    int status;
    pid_t done=waitpid(wifi_join.child,&status,WNOHANG);
    if(!done || (done<0 && errno==EINTR))return;
    wifi_join.child=0;
    ssize_t n=read(wifi_join.fd,wifi_join.reason,sizeof(wifi_join.reason));
    close(wifi_join.fd);wifi_join.fd=-1;
    wifi_join.reason[sizeof(wifi_join.reason)-1]=0;
    if(n<=0)wifi_join.reason[0]=0;
    int success=done>0 && WIFEXITED(status) && !WEXITSTATUS(status);
    wifi_join.state=success ? 2 : 3;
    if(success) {
        snprintf(wifi_ui.ssid,sizeof(wifi_ui.ssid),"%s",wifi_join.target);
        for(int i=0;i<wifi_count;i++)
            if(!strcmp(wifi_networks[i].ssid,wifi_join.target))wifi_networks[i].saved=1;
    }
}
static int wifi_join_start(int index,const char *password) {
    wifi_join_poll();
    if(wifi_join.child>0)return 0;
    int channel[2];
    if(pipe(channel))return 0;
    fcntl(channel[0],F_SETFL,O_NONBLOCK);
    pid_t pid=fork();
    if(!pid) {
        close(channel[0]);
        app_process_detach_inherited();
        signal(SIGALRM,SIG_DFL);alarm(28);
        if(!network_connect(index,password)) {
            const char *error=network_error();
            write(channel[1],error,strnlen(error,sizeof(wifi_join.reason)-1)+1);
            _exit(1);
        }
        for(int64_t until=extra_millis()+20000;extra_millis()<until;) {
            char current[128];
            if(network_status(current,sizeof(current))>0 && !strcmp(current,wifi_networks[index].ssid))_exit(0);
            struct timespec pause={.tv_nsec=350000000};
            nanosleep(&pause,NULL);
        }
        _exit(1);
    }
    close(channel[1]);
    if(pid<0){close(channel[0]);return 0;}
    wifi_join.child=pid;wifi_join.fd=channel[0];wifi_join.state=1;wifi_join.reason[0]=0;
    snprintf(wifi_join.target,sizeof(wifi_join.target),"%s",wifi_networks[index].ssid);
    return 1;
}

static void wifi_open(void) {
    wifi_ui.enabled=device_wifi_enabled();
    network_status(wifi_ui.ssid,sizeof(wifi_ui.ssid));
    if(*wifi_ui.ssid)wifi_ui.enabled=1;
    if(wifi_ui.enabled) {
        wifi_ui.scanning=network_scan();
        network_results();
    } else wifi_count=0;
}
