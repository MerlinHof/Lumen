#define _GNU_SOURCE
#include "device.h"
#include "app_packages.h"
#include "network.h"
#include "refresh.h"
#include "power_state.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#ifndef LUMEN_HOST
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <net/if.h>
#include <poll.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/prctl.h>
static int fbfd = -1, touch = -1, power = -1, lockfd = -1, eventfd = -1, usbfd = -1;
static pid_t eventpid = -1, usbpid = -1;
static int wait_ms = -1;
static PowerState power_state=POWER_AWAKE;
static PowerState emit_power;
static int64_t probe_until,probe_next,power_event_at;
static unsigned power_generation,probe_generation;
typedef struct {pid_t pid;int fd;} DeviceJob;
static DeviceJob status_job={.fd=-1},power_job={.fd=-1};
static pid_t activity_pid,reclaim_pid,sleep_pid,wake_pid;
static int64_t status_sample;
static struct {char battery[32],wifi[32];int charging;} status_cache;
static int64_t monotonic_ms(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC,&now);
    return (int64_t)now.tv_sec*1000+now.tv_nsec/1000000;
}
static void reconcile_power(void) {
    int64_t now=monotonic_ms();
    probe_next=now+150;
    if(probe_next<power_event_at+350)probe_next=power_event_at+350;
    probe_until=now+1600;
}
static void command_lifetime(void) {
    signal(SIGALRM,SIG_DFL);
    signal(SIGTERM,SIG_DFL);
    alarm(2);
}
static void reap_command(pid_t *pid) {
    if(*pid>0 && waitpid(*pid,NULL,WNOHANG)!=0)*pid=0;
}
static int start_job(DeviceJob *job) {
    int pipefd[2];
    if(job->pid || pipe2(pipefd,O_CLOEXEC|O_NONBLOCK))return -1;
    pid_t parent=getpid(),child=fork();
    if(!child) {
        close(pipefd[0]);
        prctl(PR_SET_PDEATHSIG,SIGTERM);
        if(getppid()!=parent)_exit(1);
        signal(SIGTERM,SIG_DFL);
        signal(SIGALRM,SIG_DFL);
        alarm(6);
        return pipefd[1];
    }
    close(pipefd[1]);
    if(child<0){close(pipefd[0]);return -1;}
    *job=(DeviceJob){child,pipefd[0]};
    return -1;
}
static int finish_job(DeviceJob *job,void *result,size_t size) {
    pid_t pid=job->pid;
    reap_command(&job->pid);
    if(!pid || job->pid)return 0;
    ssize_t got;
    do got=read(job->fd,result,size);while(got<0 && errno==EINTR);
    close(job->fd);
    job->fd=-1;
    return got==(ssize_t)size;
}
static unsigned char *fb;
static size_t fbsize;
static int stride, xoff, yoff;
static struct input_absinfo ax, ay;
static int xcode = ABS_MT_POSITION_X, ycode = ABS_MT_POSITION_Y;
static unsigned char previous[WIDTH * HEIGHT];
static int initialized;
static pid_t feedback_pid = -1;
int device_refresh_busy(void) {
    if (feedback_pid <= 0) return 0;
    int result=0;
    pid_t status = waitpid(feedback_pid,&result,WNOHANG);
    if (status == 0 || (status < 0 && errno == EINTR)) return 1;
    if(status<0 || !WIFEXITED(result) || WEXITSTATUS(result))initialized=0;
    feedback_pid = -1;
    return 0;
}
static void feedback_join(void) {
    if (feedback_pid > 0) {
        while (waitpid(feedback_pid, NULL, 0) < 0 && errno == EINTR) {
        }
        feedback_pid = -1;
    }
}

static volatile sig_atomic_t alive = 1, network_ready;
static void on_network(int s) {
    (void)s;
    network_ready = 1;
}
static void on_signal(int s) {
    (void)s;
    alive = 0;
}
static int execute(const char *path, char *const args[]) {
    pid_t p = fork();
    if (!p) {
        command_lifetime();
        execv(path, args);
        _exit(127);
    }
    if (p < 0)
        return -1;
    int status = 0;
    pid_t waited;
    do waited=waitpid(p,&status,0);while(waited<0 && errno==EINTR);
    if(waited!=p)return -1;
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}
static void getprop(const char *obj, const char *name, char *out, int cap) {
    if (cap < 1)
        return;
    out[0] = 0;
    int pipes[2];
    if (pipe(pipes))
        return;
    pid_t p = fork();
    if (!p) {
        command_lifetime();
        close(pipes[0]);
        dup2(pipes[1], 1);
        close(pipes[1]);
        execl("/usr/bin/lipc-get-prop", "lipc-get-prop", obj, name, NULL);
        _exit(127);
    }
    close(pipes[1]);
    struct pollfd ready={pipes[0],POLLIN,0};
    ssize_t n = p>0 && poll(&ready,1,2100)>0 ? read(pipes[0], out, (size_t)cap - 1) : -1;
    if (n > 0) {
        out[n] = 0;
        out[strcspn(out, "\r\n")] = 0;
    }
    close(pipes[0]);
    if(p>0) {
        if(n<0)kill(p,SIGKILL);
        while(waitpid(p,NULL,0)<0 && errno==EINTR){}
    }
}
static int monitor(const char *source, pid_t *pid) {
    int pipes[2];
    if (pipe(pipes))
        return -1;
    *pid = fork();
    if (!*pid) {
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        if (getppid() == 1)
            _exit(1);
        close(pipes[0]);
        dup2(pipes[1], 1);
        close(pipes[1]);
        execl("/usr/bin/lipc-wait-event", "lipc-wait-event", "-m", source, "*", NULL);
        _exit(127);
    }
    close(pipes[1]);
    fcntl(pipes[0], F_SETFL, O_NONBLOCK);
    fcntl(pipes[0], F_SETFD, FD_CLOEXEC);
    return pipes[0];
}
int device_init(void) {
    signal(SIGUSR1, on_network);
    signal(SIGTERM, on_signal);
    signal(SIGINT, on_signal);
    signal(SIGHUP, on_signal);
    signal(SIGPIPE, SIG_IGN);
    lockfd = open("/tmp/lumen.lock", O_CREAT | O_RDWR | O_CLOEXEC, 0600);
    if (lockfd < 0 || flock(lockfd, LOCK_EX | LOCK_NB))
        return 0;
    fbfd = open("/dev/fb0", O_RDWR | O_CLOEXEC);
    struct fb_var_screeninfo v;
    struct fb_fix_screeninfo f;
    if (fbfd < 0 || ioctl(fbfd, FBIOGET_VSCREENINFO, &v) || ioctl(fbfd, FBIOGET_FSCREENINFO, &f))
        return 0;
    if (v.bits_per_pixel != 8 || v.xres != WIDTH || v.yres != HEIGHT)
        return 0;
    stride = f.line_length;
    xoff = v.xoffset;
    yoff = v.yoffset;
    fbsize = f.smem_len;
    if ((size_t)(yoff + HEIGHT) * stride > fbsize)
        return 0;
    fb = mmap(NULL, fbsize, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
    if (fb == MAP_FAILED) {
        fb = NULL;
        return 0;
    }
    touch = open("/dev/input/event1", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    power = open("/dev/input/event0", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (touch < 0 || ioctl(touch, EVIOCGRAB, 1) < 0)
        return 0;
    if (ioctl(touch, EVIOCGABS(xcode), &ax) || ioctl(touch, EVIOCGABS(ycode), &ay))
        return 0;
    if (ax.maximum <= ax.minimum || ay.maximum <= ay.minimum)
        return 0;
    eventfd = monitor("com.lab126.powerd", &eventpid);
    usbfd = monitor("com.lab126.hal", &usbpid);
    reconcile_power();
    return 1;
}
void device_close(void) {
    feedback_join();
    if(status_job.pid>0)kill(status_job.pid,SIGTERM);
    if(power_job.pid>0)kill(power_job.pid,SIGTERM);
    if (touch >= 0) {
        ioctl(touch, EVIOCGRAB, 0);
        close(touch);
    }
    if (power >= 0)
        close(power);
    if (eventpid > 0) {
        kill(eventpid, SIGTERM);
        waitpid(eventpid, NULL, 0);
    }
    if (usbpid > 0) {
        kill(usbpid, SIGTERM);
        waitpid(usbpid, NULL, 0);
    }
    if (eventfd >= 0)
        close(eventfd);
    if (usbfd >= 0)
        close(usbfd);
    if (fb)
        munmap(fb, fbsize);
    if (fbfd >= 0)
        close(fbfd);
    if (lockfd >= 0)
        close(lockfd);
    struct rusage r;
    getrusage(RUSAGE_SELF, &r);
    fprintf(stderr, "peak_rss_kb=%ld user_cpu=%.3f\n", r.ru_maxrss,
            r.ru_utime.tv_sec + r.ru_utime.tv_usec / 1e6);
}
static void sample_status(void) {
    if(status_job.pid || (status_sample && monotonic_ms()-status_sample<1000))return;
    int fd=start_job(&status_job);
    if(fd<0)return;
    getprop("com.lab126.powerd","battLevel",status_cache.battery,sizeof(status_cache.battery));
    char value[64];
    getprop("com.lab126.powerd","isCharging",value,sizeof(value));
    if(*value)status_cache.charging=atoi(value)!=0;
    getprop("com.lab126.wifid","cmState",value,sizeof(value));
    char ssid[128];
    int connection=network_status(ssid,sizeof(ssid));
    if(connection>=0 || *value)
        snprintf(status_cache.wifi,sizeof(status_cache.wifi),"%s",
            (connection>=0 ? connection : !strcasecmp(value,"CONNECTED")) ? "WLAN" : "Offline");
    write(fd,&status_cache,sizeof(status_cache));
    _exit(0);
}
void device_status(char *battery,int cap,char *wifi,int nc) {
    if(*status_cache.battery)snprintf(battery,cap,"%s",status_cache.battery);
    if(*status_cache.wifi)snprintf(wifi,nc,"%s",status_cache.wifi);
    sample_status();
}
int device_charging(void) {
    sample_status();
    return status_cache.charging;
}
static void launch_command(pid_t *pid,const char *path,char *const args[]) {
    reap_command(pid);
    if(*pid>0)return;
    pid_t child=fork();
    if(!child) {
        command_lifetime();
        execv(path,args);
        _exit(127);
    }
    if(child>0)*pid=child;
}
void device_activity(void) {
    static int64_t last;
    int64_t now=monotonic_ms();
    if(last && now-last<25000)return;
    last=now;
    char *args[]={"lipc-set-prop","-i","com.lab126.powerd","touchScreenSaverTimeout","1",NULL};
    launch_command(&activity_pid,"/usr/bin/lipc-set-prop",args);
}
int device_input_pending(void) {
    struct pollfd p[2]={{touch,POLLIN,0},{power,POLLIN,0}};
    return poll(p,2,0)>0 && ((p[0].revents|p[1].revents)&POLLIN);
}
void device_poll_interval(int ms) {wait_ms=ms;}
void device_request_sleep(void) {
    char *args[]={"lipc-set-prop","-i","com.lab126.powerd","powerButton","1",NULL};
    launch_command(&sleep_pid,"/usr/bin/lipc-set-prop",args);
    reconcile_power();
}
static void reclaim_ui(void) {
    char *args[]={"sh","/mnt/us/lumen/ui-session.sh","reassert",NULL};
    launch_command(&reclaim_pid,"/bin/sh",args);
}
int device_wake(void) {
    char *args[]={"lipc-set-prop","-i","com.lab126.powerd","wakeUp","1",NULL};
    launch_command(&wake_pid,"/usr/bin/lipc-set-prop",args);
    reconcile_power();
    return wake_pid>0;
}
int device_system_power(int restart) {
    char *args[] = {restart ? "reboot" : "poweroff", NULL};
    const char *path = restart ? "/sbin/reboot" : "/sbin/poweroff";
    if (!access(path, X_OK))
        return execute(path, args) == 0;
    char *busy[] = {"busybox", restart ? "reboot" : "poweroff", NULL};
    return execute("/bin/busybox", busy) == 0;
}
void device_finish_update(void) {
    char *args[]={"lipc-set-prop","-i","com.lab126.powerd","preventScreenSaver","0",NULL};
    execute("/usr/bin/lipc-set-prop",args);
}
void device_hold_awake(int enabled) {
    static int owned;
    if (enabled && !owned) {
        char previous[16]="";
        getprop("com.lab126.powerd","preventScreenSaver",previous,sizeof(previous));
        if (!strcmp(previous,"0")) {
            char *args[]={"lipc-set-prop","-i","com.lab126.powerd","preventScreenSaver","1",NULL};
            owned=execute("/usr/bin/lipc-set-prop",args)==0;
        }
    } else if (!enabled && owned) {
        char *args[]={"lipc-set-prop","-i","com.lab126.powerd","preventScreenSaver","0",NULL};
        execute("/usr/bin/lipc-set-prop",args);
        owned=0;
    }
}
int device_wifi_enabled(void) {
    char value[32];
    getprop("com.lab126.cmd", "wirelessEnable", value, sizeof(value));
    return !strcmp(value, "1");
}
int device_wifi(int enabled) {
    char *a[] = {"lipc-set-prop",     "-i", "com.lab126.cmd", "wirelessEnable",
                 enabled ? "1" : "0", NULL};
    return execute("/usr/bin/lipc-set-prop", a) == 0;
}
void device_ip(char *out, int size) {
    if (size < 1)
        return;
    out[0] = 0;
    struct ifaddrs *ifs = NULL;
    if (getifaddrs(&ifs))
        return;
    for (struct ifaddrs *a = ifs; a; a = a->ifa_next) {
        if (!a->ifa_addr || a->ifa_addr->sa_family != AF_INET || !strcmp(a->ifa_name, "lo"))
            continue;
        if (!(a->ifa_flags & IFF_UP))
            continue;
        struct sockaddr_in *v = (struct sockaddr_in *)a->ifa_addr;
        if (!strncmp(a->ifa_name, "wlan", 4) || !strncmp(a->ifa_name, "mlan", 4) ||
            !strncmp(a->ifa_name, "eth", 3)) {
            inet_ntop(AF_INET, &v->sin_addr, out, (socklen_t)size);
            break;
        }
    }
    freeifaddrs(ifs);
}
static void present_frame(const unsigned char *pixels, int full, int asynchronous) {
    if(asynchronous && device_refresh_busy())return;
    if(!asynchronous)feedback_join();
    RefreshPlan plan=refresh_plan(previous,pixels,WIDTH,HEIGHT,STATUS_HEIGHT,initialized,full);
    if(plan.waveform==REFRESH_NONE)return;
    int top=plan.area.top,bottom=plan.area.bottom,left=plan.area.left,right=plan.area.right;
    int clear=plan.waveform==REFRESH_CLEAR;
    for (int y = top; y < bottom; y++)
        memcpy(fb + (y + yoff) * stride + xoff + left, pixels + y * WIDTH + left,
               (size_t)(right - left));
    char region[100];
    snprintf(region, sizeof(region), "top=%d,left=%d,width=%d,height=%d", top, left, right - left,
             bottom - top);
    memcpy(previous, pixels, sizeof(previous));
    initialized = 1;
    const char *waveform=plan.waveform==REFRESH_DU ? "DU" :
                         plan.waveform==REFRESH_GL16 ? "GL16" : "GC16";
    char *partial[] = {"fbink","-q","-W",(char *)waveform,"-s",region,NULL};
    char *clean[] = {"fbink", "-q", "-W", "GC16", "-f", "-s", "top=0,left=0,width=600,height=800",
                     NULL};
    char **args = clear ? clean : partial;
    const char *path = "/mnt/us/libkh/bin/fbink";
    if (!asynchronous) {
        execute(path, args);
        return;
    }
    /* The framebuffer is ready. Let fbink submit it while input keeps running. */
    pid_t parent = getpid();
    pid_t child = fork();
    if (child < 0) {
        initialized=0;
        return;
    }
    if (child == 0) {
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        if (getppid() != parent) _exit(0);
        command_lifetime();
        execv(path,args);
        _exit(127);
    }
    feedback_pid = child;
}
void device_present(const unsigned char *pixels, int full, int interval) {
    (void)interval;
    present_frame(pixels,full,0);
}
void device_present_async(const unsigned char *pixels,int full) {
    present_frame(pixels,full,1);
}
static int parse_event(int fd, int source, Input *input) {
    static char lines[2][1024];
    static size_t used[2];
    char buf[1024];
    PowerState transition=POWER_UNKNOWN;
    int status=0, usb=0;
    ssize_t n;
    while((n=read(fd,buf,sizeof(buf)))>0)for(ssize_t i=0;i<n;i++) {
        if(buf[i]!='\n') {
            if(used[source]<sizeof(lines[0])-1)lines[source][used[source]++]=buf[i];
            continue;
        }
        char *line=lines[source];
        line[used[source]]=0;
        used[source]=0;
        PowerState event=source==0 ? power_state_event(line) : POWER_UNKNOWN;
        if(event!=POWER_UNKNOWN)transition=event;
        if(strstr(line,"charging") || strstr(line,"Charging") || strstr(line,"usbUnplug") ||
           strstr(line,"usbPlugOut"))status=1;
        if(source==1 && (strstr(line,"usbConfigured") || strstr(line,"usbPlugIn") ||
           strstr(line,"enteringUsbMs") || strstr(line,"UsbMode") || strstr(line,"usbStateChanged")))usb=1;
    }
    if(usb){input->kind=INPUT_USB;return 1;}
    if(transition!=POWER_UNKNOWN) {
        emit_power=transition;
        power_generation++;
        power_event_at=monotonic_ms();
        probe_until=0;
        return 0;
    }
    if(status){input->kind=INPUT_STATUS;input->x=-1;return 1;}
    return 0;
}
static int sync_power(Input *input) {
    PowerState state=emit_power;
    emit_power=POWER_UNKNOWN;
    PowerState observed;
    if(finish_job(&power_job,&observed,sizeof(observed)) && state==POWER_UNKNOWN &&
       probe_generation==power_generation)state=observed;
    int64_t now=monotonic_ms();
    if(!power_job.pid && now>=probe_next && now<=probe_until) {
        probe_next=now+200;
        probe_generation=power_generation;
        int fd=start_job(&power_job);
        if(fd>=0) {
            char status[128];
            getprop("com.lab126.powerd","status",status,sizeof(status));
            observed=power_state_status(status);
            write(fd,&observed,sizeof(observed));
            _exit(0);
        }
    }
    if(state==POWER_UNKNOWN || state==power_state)return 0;
    power_state=state;
    if(state==POWER_AWAKE) {
        reclaim_ui();
        initialized=0;
    }
    input->kind=state==POWER_AWAKE ? INPUT_WAKE : INPUT_SLEEP;
    return 1;
}
int device_wait(Input *input) {
    static int x, y, sx, sy, active, started, release, held, desynced;
    static int64_t began, power_began;
    static int power_active, power_sent;
    memset(input, 0, sizeof(*input));
    struct pollfd p[6+APP_OVERLAY_COUNT] = {
        {touch, POLLIN, 0}, {power, POLLIN, 0}, {eventfd, POLLIN, 0}, {usbfd, POLLIN, 0}, {-1, POLLIN, 0}, {-1, POLLIN, 0}};
    while (alive) {
        if(sync_power(input)) {
            active=started=release=held=desynced=0;
            return 1;
        }
        reap_command(&activity_pid);
        reap_command(&reclaim_pid);
        reap_command(&sleep_pid);
        reap_command(&wake_pid);
        if(finish_job(&status_job,&status_cache,sizeof(status_cache))) {
            status_sample=monotonic_ms();
            input->kind=INPUT_STATUS;
            input->x=-1;
            return 1;
        }
        if (network_ready) {
            network_ready = 0;
            input->kind = INPUT_NETWORK;
            return 1;
        }
        int timeout = wait_ms;
        int64_t now=monotonic_ms();
        if((status_job.pid || power_job.pid || now<=probe_until) && (timeout<0 || timeout>40))timeout=40;
        if (active && started && !held) {
            struct timespec t;
            clock_gettime(CLOCK_MONOTONIC, &t);
            int64_t now = (int64_t)t.tv_sec * 1000 + t.tv_nsec / 1000000;
            int remaining = (int)(began + 550 - now);
            if (remaining < 0)
                remaining = 0;
            if (timeout < 0 || remaining < timeout)
                timeout = remaining;
        }
        if (power_active && !power_sent) {
            struct timespec t;
            clock_gettime(CLOCK_MONOTONIC, &t);
            int64_t now = (int64_t)t.tv_sec * 1000 + t.tv_nsec / 1000000;
            int remain = power_began + 900 - now;
            if (remain <= 0) {
                power_sent = 1;
                input->kind = INPUT_POWER;
                input->x = 2;
                return 1;
            }
            if (timeout < 0 || remain < timeout)
                timeout = remain;
        }
        p[4].fd = app_process_fd();
        for(int i=0;i<APP_OVERLAY_COUNT;i++)p[5+i]=(struct pollfd){app_overlay_fd(i),POLLIN,0};
        p[5+APP_OVERLAY_COUNT]=(struct pollfd){app_service_fd(),POLLIN,0};
        int service_wait=app_service_wait_ms();
        if(service_wait>=0 && (timeout<0 || service_wait<timeout))timeout=service_wait;
        int n=poll(p,6+APP_OVERLAY_COUNT,timeout);
        if (!n) {
            if (active && started && !held) {
                struct timespec t;
                clock_gettime(CLOCK_MONOTONIC, &t);
                int64_t now = (int64_t)t.tv_sec * 1000 + t.tv_nsec / 1000000;
                if (now - began >= 550) {
                    held = 1;
                    input->kind = INPUT_HOLD;
                    input->x = x;
                    input->y = y;
                    return 1;
                }
            }
            input->kind = INPUT_STATUS;
            input->x = 0;
            return 1;
        }
        if (n < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        for (int k = 2; k < 4; k++) {
            if (p[k].revents & POLLIN) {
                if (parse_event(p[k].fd, k - 2, input))
                    return 1;
            } else if (p[k].revents & (POLLHUP | POLLERR)) {
                close(p[k].fd);
                if (k == 2)
                    eventfd = -1;
                else
                    usbfd = -1;
                p[k].fd = -1;
            }
        }
        if(sync_power(input)) {
            active=started=release=held=desynced=0;
            return 1;
        }
        for (int k = 1; k >= 0; k--) {
            if (!(p[k].revents & POLLIN))
                continue;
            struct input_event e;
            while (read(p[k].fd, &e, sizeof(e)) == (ssize_t)sizeof(e)) {
                if (k == 1) {
                    if(e.type==EV_SYN && e.code==SYN_DROPPED) {
                        power_active=power_sent=0;
                        reconcile_power();
                        input->kind=INPUT_POWER;
                        input->x=0;
                        return 1;
                    }
                    if (e.type == EV_KEY && e.code == KEY_POWER) {
                        fprintf(stderr, "physical_power=%d\n", e.value);
                        if (e.value == 0 || e.value == 1) {
                            if (e.value == 1) {
                                struct timespec t;
                                clock_gettime(CLOCK_MONOTONIC, &t);
                                power_began = (int64_t)t.tv_sec * 1000 + t.tv_nsec / 1000000;
                                power_active = 1;
                                power_sent = 0;
                            } else {
                                power_active = 0;
                                reconcile_power();
                            }
                            input->kind = INPUT_POWER;
                            input->x = e.value;
                            return 1;
                        }
                    }
                    continue;
                }
                if (e.type == EV_SYN && e.code == SYN_DROPPED) {
                    active = started = release = held = 0;
                    desynced = 1;
                    input->kind = INPUT_CANCEL;
                    return 1;
                }
                if (desynced) {
                    if (e.type == EV_SYN && e.code == SYN_REPORT) desynced = 0;
                    continue;
                }
                if (e.type == EV_ABS) {
                    if (e.code == xcode)
                        x = (e.value - ax.minimum) * (WIDTH - 1) / (ax.maximum - ax.minimum);
                    if (e.code == ycode)
                        y = (e.value - ay.minimum) * (HEIGHT - 1) / (ay.maximum - ay.minimum);
                    if (e.code == ABS_MT_TRACKING_ID) {
                        if (e.value < 0)
                            release = 1;
                        else {
                            active = 1;
                            started = 0;
                        }
                    }
                }
                if (e.type == EV_KEY && e.code == BTN_TOUCH) {
                    if (e.value)
                        active = 1;
                    else
                        release = 1;
                }
                if (e.type == EV_SYN && e.code == SYN_REPORT) {
                    if (active && !release) {
                        input->x = x;
                        input->y = y;
                        if (!started && power_state==POWER_ASLEEP) {
                            reconcile_power();
                            if(sync_power(input)) {
                                active=started=release=held=desynced=0;
                                return 1;
                            }
                        }
                        if (!started) {
                            sx = x;
                            sy = y;
                            started = 1;
                            held = 0;
                            struct timespec t;
                            clock_gettime(CLOCK_MONOTONIC, &t);
                            began = (int64_t)t.tv_sec * 1000 + t.tv_nsec / 1000000;
                            input->kind = INPUT_DOWN;
                        } else {
                            if (!held && (abs(x - sx) > 18 || abs(y - sy) > 18))
                                held = 1;
                            input->kind = INPUT_MOVE;
                        }
                        return 1;
                    }
                    if (release && !started) {
                        active = release = held = 0;
                        continue;
                    }
                    if (release) {
                        input->x = x;
                        input->y = y;
                        input->dx = x - sx;
                        input->dy = y - sy;
                        input->kind = started && (abs(input->dx) > 65 || abs(input->dy) > 80)
                                          ? INPUT_SWIPE
                                          : INPUT_TAP;
                        active = started = release = held = 0;
                        return 1;
                    }
                }
            }
        }
        for(int i=0;i<APP_OVERLAY_COUNT;i++)if(p[5+i].revents&(POLLIN|POLLHUP|POLLERR)) {
            input->kind=INPUT_OVERLAY;input->x=i;return 1;
        }
        if (p[5+APP_OVERLAY_COUNT].revents & (POLLIN | POLLHUP | POLLERR)) {
            input->kind=INPUT_APP_SERVICE;
            return 1;
        }
        if (p[4].revents & (POLLIN | POLLHUP | POLLERR)) {
            input->kind = INPUT_APP;
            return 1;
        }
    }
    input->kind = INPUT_EXIT;
    return 1;
}
#else
int device_input_pending(void) { return 0; }
int device_refresh_busy(void) { return 0; }
void device_poll_interval(int ms) { (void)ms; }
int device_init(void) { return 1; }
void device_close(void) {}
void device_status(char *b, int n, char *w, int nc) {
    snprintf(b, n, "68");
    snprintf(w, nc, "Offline");
}
int device_charging(void) {
    const char *s = getenv("LUMEN_CHARGING");
    return s && atoi(s);
}
void device_activity(void) {}
void device_request_sleep(void) {}
int device_wake(void) { return 1; }
int device_system_power(int restart) {
    (void)restart;
    return 1;
}
void device_hold_awake(int enabled) { (void)enabled; }
void device_finish_update(void) {}
int device_wifi_enabled(void) { return 1; }
int device_wifi(int e) {
    (void)e;
    return 1;
}
void device_ip(char *s, int n) { snprintf(s, n, "127.0.0.1"); }
void device_present_async(const unsigned char *p,int full) { (void)p;(void)full; }
void device_present(const unsigned char *p, int f, int i) {
    (void)p;
    (void)f;
    (void)i;
}
int device_wait(Input *in) {
    in->kind = INPUT_EXIT;
    return 1;
}
#endif
