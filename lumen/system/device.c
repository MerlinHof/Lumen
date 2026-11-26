#define _GNU_SOURCE
#include "virtual_device.h"
#include "app_packages.h"
#include "network.h"
#include "refresh.h"
#include "display_viewport.h"
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

static int virtual_hardware;
int device_emulated(void) {return virtual_hardware;}
int device_bluetooth_remote(void) {return virtual_hardware && virtual_device_bluetooth_remote();}
int device_bluetooth_connection(void) {return virtual_hardware ? virtual_device_bluetooth_connection() : -1;}
int device_bluetooth_connect(int connect) {return virtual_hardware && virtual_device_bluetooth_connect(connect);}
#ifndef LUMEN_HOST
#include <arpa/inet.h>
#include <dirent.h>
#include <linux/rfkill.h>
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
static int stride,xoff,yoff,pixel_bytes;
static DisplayViewport viewport;
static uint32_t gray_palette[256];
static uint16_t source_column[8192],source_row[8192];
static struct input_absinfo ax, ay;
static int xcode = ABS_MT_POSITION_X, ycode = ABS_MT_POSITION_Y;
static unsigned char previous[WIDTH * HEIGHT];
static int initialized;
static pid_t feedback_pid = -1;
int device_refresh_busy(void) {
    if(virtual_hardware)return virtual_device_refresh_busy();

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
static int bit_set(const unsigned long *bits,unsigned index) {
    const unsigned width=sizeof(*bits)*8;
    return !!(bits[index/width] & (1ul<<(index%width)));
}
static int touch_axes(int fd,struct input_absinfo *x,struct input_absinfo *y,int *xc,int *yc) {
    int xs[]={ABS_MT_POSITION_X,ABS_X},ys[]={ABS_MT_POSITION_Y,ABS_Y};
    for(int i=0;i<2;i++)if(!ioctl(fd,EVIOCGABS(xs[i]),x) &&
       !ioctl(fd,EVIOCGABS(ys[i]),y) && x->maximum>x->minimum && y->maximum>y->minimum) {
        *xc=xs[i];*yc=ys[i];
        return 1;
    }
    return 0;
}
static int open_inputs(void) {
    int best=-1;
    for(int i=0;i<64;i++) {
        char path[48];
        snprintf(path,sizeof(path),"/dev/input/event%d",i);
        int fd=open(path,O_RDONLY|O_NONBLOCK|O_CLOEXEC);
        if(fd<0)continue;
        unsigned long keys[(KEY_MAX+sizeof(long)*8)/(sizeof(long)*8)]={0};
        unsigned long props[(INPUT_PROP_MAX+sizeof(long)*8)/(sizeof(long)*8)]={0};
        unsigned long axes[(ABS_MAX+sizeof(long)*8)/(sizeof(long)*8)]={0};
        ioctl(fd,EVIOCGBIT(EV_KEY,sizeof(keys)),keys);
        ioctl(fd,EVIOCGBIT(EV_ABS,sizeof(axes)),axes);
        ioctl(fd,EVIOCGPROP(sizeof(props)),props);
        struct input_absinfo tx,ty;
        int xc,yc;
        int direct=bit_set(props,INPUT_PROP_DIRECT);
        int tracks=bit_set(axes,ABS_MT_TRACKING_ID);
        if(!bit_set(props,INPUT_PROP_POINTER) && (tracks || bit_set(keys,BTN_TOUCH)) &&
           touch_axes(fd,&tx,&ty,&xc,&yc)) {
            int score=direct*2+tracks;
            if(score>best) {
                if(touch>=0)close(touch);
                touch=fd;ax=tx;ay=ty;xcode=xc;ycode=yc;best=score;
                continue;
            }
        }
        if(power<0 && bit_set(keys,KEY_POWER)){power=fd;continue;}
        close(fd);
    }
    return touch>=0 && !ioctl(touch,EVIOCGRAB,1);
}
static int display_format(const struct fb_var_screeninfo *v,const struct fb_fix_screeninfo *f) {
    if(f->type!=FB_TYPE_PACKED_PIXELS || v->nonstd)return 0;
    if(v->bits_per_pixel==8 && (v->grayscale==1 ||
       (v->xres==WIDTH && v->yres==HEIGHT) || f->visual==FB_VISUAL_STATIC_PSEUDOCOLOR ||
       f->visual==FB_VISUAL_PSEUDOCOLOR)) {
        pixel_bytes=1;
        for(unsigned i=0;i<256;i++)gray_palette[i]=i;
        return 1;
    }
    if(f->visual!=FB_VISUAL_TRUECOLOR ||
       (v->bits_per_pixel!=16 && v->bits_per_pixel!=24 && v->bits_per_pixel!=32))return 0;
    const struct fb_bitfield fields[]={v->red,v->green,v->blue,v->transp};
    DisplayChannel channels[4];
    for(int i=0;i<4;i++) {
        if(fields[i].length && fields[i].msb_right)return 0;
        channels[i]=(DisplayChannel){fields[i].offset,fields[i].length};
    }
    if(!display_gray_palette(v->bits_per_pixel,channels,gray_palette))return 0;
    pixel_bytes=v->bits_per_pixel/8;
    return 1;
}
int device_init(void) {
    virtual_hardware=getenv("LUMEN_EMULATOR_TTY") || getenv("LUMEN_EMULATOR_DISPLAY_FD");
    if(virtual_hardware)return virtual_device_init();

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
    if(v.xres>8192 || v.yres>8192 || !display_viewport(&viewport,(int)v.xres,(int)v.yres) ||
       !display_format(&v,&f) || (uint64_t)(v.xoffset+(uint64_t)v.xres)*pixel_bytes>f.line_length ||
       ((uint64_t)v.yoffset+v.yres)*f.line_length>f.smem_len) {
        fprintf(stderr,"Unsupported framebuffer: %ux%u at %u bits, visual %u\n",
                v.xres,v.yres,v.bits_per_pixel,f.visual);
        return 0;
    }
    for(int x=0;x<viewport.box.w;x++)source_column[x]=(uint16_t)(x*WIDTH/viewport.box.w);
    for(int y=0;y<viewport.box.h;y++)source_row[y]=(uint16_t)(y*HEIGHT/viewport.box.h);
    stride=f.line_length;xoff=v.xoffset;yoff=v.yoffset;fbsize=f.smem_len;
    fb=mmap(NULL,fbsize,PROT_READ|PROT_WRITE,MAP_SHARED,fbfd,0);
    if(fb==MAP_FAILED){fb=NULL;return 0;}
    if(!open_inputs())return 0;
    fprintf(stderr,"Panel %dx%d, logical viewport %dx%d at %d,%d; power input %s\n",
        viewport.panel_w,viewport.panel_h,viewport.box.w,viewport.box.h,viewport.box.x,viewport.box.y,
        power>=0 ? "available" : "firmware events");
    eventfd = monitor("com.lab126.powerd", &eventpid);
    usbfd = monitor("com.lab126.hal", &usbpid);
    reconcile_power();
    return 1;
}
void device_close(void) {
    if(virtual_hardware){virtual_device_close();return;}

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
    if(virtual_hardware){virtual_device_status(battery,cap,wifi,nc);return;}

    if(*status_cache.battery)snprintf(battery,cap,"%s",status_cache.battery);
    if(*status_cache.wifi)snprintf(wifi,nc,"%s",status_cache.wifi);
    sample_status();
}
int device_charging(void) {
    if(virtual_hardware)return virtual_device_charging();

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
    if(virtual_hardware){virtual_device_activity();return;}

    static int64_t last;
    int64_t now=monotonic_ms();
    if(last && now-last<25000)return;
    last=now;
    char *args[]={"lipc-set-prop","-i","com.lab126.powerd","touchScreenSaverTimeout","1",NULL};
    launch_command(&activity_pid,"/usr/bin/lipc-set-prop",args);
}
int device_input_pending(void) {
    if(virtual_hardware)return virtual_device_input_pending();

    struct pollfd p[2]={{touch,POLLIN,0},{power,POLLIN,0}};
    return poll(p,2,0)>0 && ((p[0].revents|p[1].revents)&POLLIN);
}
void device_poll_interval(int ms) {
    if(virtual_hardware){virtual_device_poll_interval(ms);return;}
wait_ms=ms;}
void device_request_sleep(void) {
    if(virtual_hardware){virtual_device_request_sleep();return;}

    char *args[]={"lipc-set-prop","-i","com.lab126.powerd","powerButton","1",NULL};
    launch_command(&sleep_pid,"/usr/bin/lipc-set-prop",args);
    reconcile_power();
}
static void reclaim_ui(void) {
    char *args[]={"sh","/mnt/us/lumen/ui-session.sh","reassert",NULL};
    launch_command(&reclaim_pid,"/bin/sh",args);
}
int device_wake(void) {
    if(virtual_hardware)return virtual_device_wake();

    char *args[]={"lipc-set-prop","-i","com.lab126.powerd","wakeUp","1",NULL};
    launch_command(&wake_pid,"/usr/bin/lipc-set-prop",args);
    reconcile_power();
    return wake_pid>0;
}
int device_system_power(int restart) {
    if(virtual_hardware)return virtual_device_system_power(restart);

    char *args[] = {restart ? "reboot" : "poweroff", NULL};
    const char *path = restart ? "/sbin/reboot" : "/sbin/poweroff";
    if (!access(path, X_OK))
        return execute(path, args) == 0;
    char *busy[] = {"busybox", restart ? "reboot" : "poweroff", NULL};
    return execute("/bin/busybox", busy) == 0;
}
void device_finish_update(void) {
    if(virtual_hardware){virtual_device_finish_update();return;}

    char *args[]={"lipc-set-prop","-i","com.lab126.powerd","preventScreenSaver","0",NULL};
    execute("/usr/bin/lipc-set-prop",args);
}
void device_hold_awake(int enabled) {
    if(virtual_hardware){virtual_device_hold_awake(enabled);return;}

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
    if(virtual_hardware)return virtual_device_wifi_enabled();

    char value[32];
    getprop("com.lab126.cmd", "wirelessEnable", value, sizeof(value));
    return !strcmp(value, "1");
}
int device_wifi(int enabled) {
    if(virtual_hardware)return virtual_device_wifi(enabled);

    char *a[] = {"lipc-set-prop",     "-i", "com.lab126.cmd", "wirelessEnable",
                 enabled ? "1" : "0", NULL};
    return execute("/usr/bin/lipc-set-prop", a) == 0;
}
static int sysfs_text(const char *path,char *out,size_t capacity) {
    int fd=open(path,O_RDONLY|O_CLOEXEC);
    if(fd<0)return 0;
    ssize_t n=read(fd,out,capacity-1);
    close(fd);
    if(n<=0)return 0;
    out[n]=0;
    out[strcspn(out,"\r\n")]=0;
    return 1;
}
static int sysfs_number(const char *path) {
    char text[32],*end;
    if(!sysfs_text(path,text,sizeof(text)))return -1;
    long value=strtol(text,&end,10);
    return end!=text && !*end && value>=0 && value<=100000 ? (int)value : -1;
}
static int backlight_path(char path[256],int *current) {
    DIR *dir=opendir("/sys/class/backlight");
    if(!dir)return -1;
    int max=-1;
    for(struct dirent *ent;(ent=readdir(dir));) {
        if(ent->d_name[0]=='.' || strlen(ent->d_name)>100)continue;
        char file[256];
        snprintf(file,sizeof(file),"/sys/class/backlight/%s/max_brightness",ent->d_name);
        int available=sysfs_number(file);
        if(available<=0)continue;
        snprintf(file,sizeof(file),"/sys/class/backlight/%s/brightness",ent->d_name);
        int level=sysfs_number(file);
        if(level<0 || level>available || access(file,W_OK))continue;
        /* An ambiguous driver list needs a device-specific backend. */
        if(max>=0){max=-1;break;}
        max=available;*current=level;
        snprintf(path,256,"%s",file);
    }
    closedir(dir);
    return max;
}
static int bluetooth_radio(int *enabled) {
    if(access("/dev/rfkill",W_OK))return -1;
    DIR *dir=opendir("/sys/class/rfkill");
    if(!dir)return -1;
    int index=-1;
    for(struct dirent *ent;(ent=readdir(dir));) {
        if(strncmp(ent->d_name,"rfkill",6))continue;
        char *end,type[32],path[256];
        long n=strtol(ent->d_name+6,&end,10);
        if(end==ent->d_name+6 || *end || n<0 || n>100000)continue;
        snprintf(path,sizeof(path),"/sys/class/rfkill/rfkill%ld/type",n);
        if(!sysfs_text(path,type,sizeof(type)) || strcmp(type,"bluetooth"))continue;
        snprintf(path,sizeof(path),"/sys/class/rfkill/rfkill%ld/hard",n);
        if(sysfs_number(path)!=0)continue;
        snprintf(path,sizeof(path),"/sys/class/rfkill/rfkill%ld/soft",n);
        int soft=sysfs_number(path);
        if(soft<0 || soft>1)continue;
        if(index>=0){index=-1;break;}
        index=(int)n;*enabled=!soft;
    }
    closedir(dir);
    return index;
}
DeviceCapabilities device_capabilities(void) {
    if(virtual_hardware)return virtual_device_capabilities();

    DeviceCapabilities caps={-1,-1};
    char path[256];
    int current;
    int max=backlight_path(path,&current);
    if(max>0)caps.brightness=(current*100+max/2)/max;
    if(bluetooth_radio(&current)>=0)caps.bluetooth=current;
    return caps;
}
int device_brightness(int percent) {
    if(virtual_hardware)return virtual_device_brightness(percent);

    if(percent<0 || percent>100)return 0;
    char path[256],value[16];
    int current,max=backlight_path(path,&current);
    if(max<=0)return 0;
    int length=snprintf(value,sizeof(value),"%d\n",(percent*max+50)/100);
    int fd=open(path,O_WRONLY|O_CLOEXEC);
    if(fd<0)return 0;
    ssize_t n=write(fd,value,(size_t)length);
    close(fd);
    return n==length;
}
int device_bluetooth(int enabled) {
    if(virtual_hardware)return virtual_device_bluetooth(enabled);

    if(enabled!=0 && enabled!=1)return 0;
    int current,index=bluetooth_radio(&current);
    if(index<0)return 0;
    int fd=open("/dev/rfkill",O_WRONLY|O_NONBLOCK|O_CLOEXEC);
    if(fd<0)return 0;
    struct rfkill_event event={.idx=(uint32_t)index,.type=RFKILL_TYPE_BLUETOOTH,
        .op=RFKILL_OP_CHANGE,.soft=!enabled};
    ssize_t n=write(fd,&event,sizeof(event));
    close(fd);
    return n==(ssize_t)sizeof(event);
}
void device_ip(char *out, int size) {
    if(virtual_hardware){virtual_device_ip(out,size);return;}

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
    int clear=plan.waveform==REFRESH_CLEAR || !initialized;
    LumenRect damage=display_damage(&viewport,(LumenRect){plan.area.left,plan.area.top,
        plan.area.right-plan.area.left,plan.area.bottom-plan.area.top});
    if(clear) {
        for(int y=0;y<viewport.panel_h;y++) {
            unsigned char *dest=fb+(size_t)(y+yoff)*stride+(size_t)xoff*pixel_bytes;
            memset(dest,255,(size_t)viewport.panel_w*pixel_bytes);
        }
        damage=viewport.box;
    }
    if(!damage.w || !damage.h) {
        memcpy(previous,pixels,sizeof(previous));
        return;
    }
    for(int y=damage.y;y<damage.y+damage.h;y++) {
        int sy=source_row[y-viewport.box.y];
        unsigned char *dest=fb+(size_t)(y+yoff)*stride+(size_t)(damage.x+xoff)*pixel_bytes;
        if(pixel_bytes==1 && viewport.box.w==WIDTH) {
            memcpy(dest,pixels+sy*WIDTH+damage.x-viewport.box.x,(size_t)damage.w);
            continue;
        }
        for(int x=damage.x;x<damage.x+damage.w;x++) {
            int sx=source_column[x-viewport.box.x];
            uint32_t sample=gray_palette[pixels[sy*WIDTH+sx]];
            for(int byte=0;byte<pixel_bytes;byte++)*dest++=(unsigned char)(sample>>(8*byte));
        }
    }
    LumenRect dirty=clear ? (LumenRect){0,0,viewport.panel_w,viewport.panel_h} : damage;
    char region[100];
    snprintf(region,sizeof(region),"top=%d,left=%d,width=%d,height=%d",dirty.y,dirty.x,dirty.w,dirty.h);
    memcpy(previous, pixels, sizeof(previous));
    initialized = 1;
    const char *waveform=plan.waveform==REFRESH_DU ? "DU" :
                         plan.waveform==REFRESH_GL16 ? "GL16" : "GC16";
    char *partial[] = {"fbink","-q","-W",(char *)waveform,"-s",region,NULL};
    char *clean[] = {"fbink", "-q", "-W", "GC16", "-f", "-s",region,
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
    if(virtual_hardware){virtual_device_present(pixels,full,interval);return;}

    (void)interval;
    present_frame(pixels,full,0);
}
void device_present_async(const unsigned char *pixels,int full) {
    if(virtual_hardware){virtual_device_present_async(pixels,full);return;}

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
    if(virtual_hardware)return virtual_device_wait(input);

    static int x,y,px,py,sx,sy,active,started,release,held,desynced,blocked;
    static int64_t began, power_began;
    static int power_active,power_sent,slot;
    memset(input, 0, sizeof(*input));
    struct pollfd p[6+APP_OVERLAY_COUNT] = {
        {touch, POLLIN, 0}, {power, POLLIN, 0}, {eventfd, POLLIN, 0}, {usbfd, POLLIN, 0}, {-1, POLLIN, 0}, {-1, POLLIN, 0}};
    while (alive) {
        if(sync_power(input)) {
            active=started=release=held=desynced=blocked=0;
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
            active=started=release=held=desynced=blocked=0;
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
                    active=started=release=held=blocked=0;
                    desynced = 1;
                    input->kind = INPUT_CANCEL;
                    return 1;
                }
                if (desynced) {
                    if (e.type == EV_SYN && e.code == SYN_REPORT) desynced = 0;
                    continue;
                }
                if (e.type == EV_ABS) {
                    if(e.code==ABS_MT_SLOT){slot=e.value;continue;}
                    if(slot)continue;
                    if(e.code==xcode) {
                        px=display_axis(e.value,ax.minimum,ax.maximum,viewport.panel_w);
                        x=display_to_logical(px,viewport.box.x,viewport.box.w,WIDTH);
                    }
                    if(e.code==ycode) {
                        py=display_axis(e.value,ay.minimum,ay.maximum,viewport.panel_h);
                        y=display_to_logical(py,viewport.box.y,viewport.box.h,HEIGHT);
                    }
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
                                active=started=release=held=desynced=blocked=0;
                                return 1;
                            }
                        }
                        if(!started && !display_contains(&viewport,px,py))blocked=1;
                        if(blocked)continue;
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
                        active=release=held=blocked=0;
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
                        active=started=release=held=blocked=0;
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
int device_input_pending(void) {
    if(virtual_hardware)return virtual_device_input_pending();
     return 0; }
int device_refresh_busy(void) {
    if(virtual_hardware)return virtual_device_refresh_busy();
     return 0; }
void device_poll_interval(int ms) {
    if(virtual_hardware){virtual_device_poll_interval(ms);return;}
     (void)ms; }
int device_init(void) {
    virtual_hardware=getenv("LUMEN_EMULATOR_TTY") || getenv("LUMEN_EMULATOR_DISPLAY_FD");
    if(virtual_hardware)return virtual_device_init();
     return 1; }
void device_close(void) {
    if(virtual_hardware){virtual_device_close();return;}
}
void device_status(char *b, int n, char *w, int nc) {
    if(virtual_hardware){virtual_device_status(b,n,w,nc);return;}

    snprintf(b, n, "68");
    snprintf(w, nc, "Offline");
}
int device_charging(void) {
    if(virtual_hardware)return virtual_device_charging();

    const char *s = getenv("LUMEN_CHARGING");
    return s && atoi(s);
}
void device_activity(void) {
    if(virtual_hardware){virtual_device_activity();return;}
}
void device_request_sleep(void) {
    if(virtual_hardware){virtual_device_request_sleep();return;}
}
int device_wake(void) {
    if(virtual_hardware)return virtual_device_wake();
     return 1; }
int device_system_power(int restart) {
    if(virtual_hardware)return virtual_device_system_power(restart);

    (void)restart;
    return 1;
}
void device_hold_awake(int enabled) {
    if(virtual_hardware){virtual_device_hold_awake(enabled);return;}
     (void)enabled; }
void device_finish_update(void) {
    if(virtual_hardware){virtual_device_finish_update();return;}
}
DeviceCapabilities device_capabilities(void) {
    if(virtual_hardware)return virtual_device_capabilities();
     return (DeviceCapabilities){-1,-1}; }
int device_brightness(int percent) {
    if(virtual_hardware)return virtual_device_brightness(percent);
     (void)percent;return 0; }
int device_bluetooth(int enabled) {
    if(virtual_hardware)return virtual_device_bluetooth(enabled);
     (void)enabled;return 0; }
int device_wifi_enabled(void) {
    if(virtual_hardware)return virtual_device_wifi_enabled();
     return 1; }
int device_wifi(int e) {
    if(virtual_hardware)return virtual_device_wifi(e);

    (void)e;
    return 1;
}
void device_ip(char *s, int n) {
    if(virtual_hardware){virtual_device_ip(s,n);return;}
     snprintf(s, n, "127.0.0.1"); }
void device_present_async(const unsigned char *p,int full) {
    if(virtual_hardware){virtual_device_present_async(p,full);return;}
     (void)p;(void)full; }
void device_present(const unsigned char *p, int f, int i) {
    if(virtual_hardware){virtual_device_present(p,f,i);return;}

    (void)p;
    (void)f;
    (void)i;
}
int device_wait(Input *in) {
    if(virtual_hardware)return virtual_device_wait(in);

    in->kind = INPUT_EXIT;
    return 1;
}
#endif
