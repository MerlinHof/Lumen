/* Touch and grayscale frame transport for the emulator. */
#define _POSIX_C_SOURCE 200809L
#include "virtual_device.h"
#include "refresh.h"
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <poll.h>
#include "app_packages.h"
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#ifndef LUMEN_HOST
#include <sys/mount.h>
#endif

enum { TOUCH_DOWN = 1, TOUCH_MOVE, TOUCH_UP, TOUCH_CANCEL, POWER_DOWN, POWER_UP, STOP, BLUETOOTH_RESULT };
typedef struct { int32_t kind, x, y; } Packet;
static int input_fd=STDIN_FILENO,display_fd=-1, timeout_ms = -1, asleep, pending_sleep, pending_power_release;
static int brightness=70,bluetooth=1,wifi_on=1,bluetooth_remote,bluetooth_connection;
static int touching, held, touch_x, touch_y, start_x, start_y;
static int power_pressed, power_held;
static int64_t touch_started, power_started;

static int64_t milliseconds(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}
static int read_packet(Packet *packet) {
    unsigned char *destination = (unsigned char *)packet;
    size_t remaining = sizeof(*packet);
    while (remaining) {
        ssize_t n = read(input_fd, destination, remaining);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return 0;
        destination += n;
        remaining -= (size_t)n;
    }
    return 1;
}
static int write_all(const unsigned char *data, size_t remaining) {
    while (remaining) {
        ssize_t n = write(display_fd, data, remaining);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return 0;
        data += n;
        remaining -= (size_t)n;
    }
    return 1;
}
static void present(const unsigned char *pixels, int full) {
    static unsigned char previous[WIDTH * HEIGHT];
    static int initialized;
    RefreshPlan plan=refresh_plan(previous,pixels,WIDTH,HEIGHT,STATUS_HEIGHT,initialized,full);
    if(plan.waveform==REFRESH_NONE)return;
    /* Send only the changed rectangle over the emulated serial link. */
    static unsigned char frame[10+WIDTH*HEIGHT];
    static const unsigned char magic[]={0x4c,0x55,0x4d,0x45,0x4e,0x46,0x42,0x02};
    if(!initialized && !write_all(magic,sizeof(magic)))return;
    RefreshBounds box=plan.area;
    int values[]={box.left,box.top,box.right,box.bottom};
    frame[0]=(unsigned char)plan.waveform;
    frame[1]=(unsigned char)brightness;
    for(int i=0;i<4;i++) {
        frame[2+2*i]=(unsigned char)values[i];
        frame[3+2*i]=(unsigned char)(values[i]>>8);
    }
    size_t size=10;
    int width=box.right-box.left;
    for(int y=box.top;y<box.bottom;y++,size+=width)
        memcpy(frame+size,pixels+y*WIDTH+box.left,width);
    if(write_all(frame,size)) {
        memcpy(previous,pixels,sizeof(previous));
        initialized=1;
    }
}

#ifndef LUMEN_HOST
int virtual_device_guest_bootstrap(void) {
    if(mount("devtmpfs","/dev","devtmpfs",0,NULL)!=0 ||
       mount("proc","/proc","proc",0,NULL)!=0 ||
       mount("sysfs","/sys","sysfs",0,NULL)!=0 ||
       setenv("LUMEN_EMULATOR_TTY","/dev/ttyAMA0",1)!=0)return 0;
    const char *epoch=getenv("LUMEN_EMULATOR_EPOCH");
    if(epoch) {
        struct timespec now={.tv_sec=(time_t)strtoll(epoch,NULL,10)};
        if(now.tv_sec>0 && clock_settime(CLOCK_REALTIME,&now)!=0)return 0;
    }
    int sink=open("/dev/null",O_RDWR);
    if(sink<0)return 0;
    int ok=dup2(sink,STDIN_FILENO)>=0 && dup2(sink,STDOUT_FILENO)>=0 && dup2(sink,STDERR_FILENO)>=0;
    if(sink>STDERR_FILENO)close(sink);
    return ok;
}
#endif

int virtual_device_init(void) {
    const char *tty=getenv("LUMEN_EMULATOR_TTY");
    const char *fd=getenv("LUMEN_EMULATOR_DISPLAY_FD");
    if(tty) {
        input_fd=open(tty,O_RDWR|O_NOCTTY|O_CLOEXEC);
        if(input_fd<0)return 0;
        struct termios mode={0};
        if(tcgetattr(input_fd,&mode)!=0)goto failed;
        mode.c_iflag=0;mode.c_oflag=0;mode.c_lflag=0;
        mode.c_cflag=(mode.c_cflag & ~(CSIZE|PARENB))|CS8|CREAD|CLOCAL;
        mode.c_cc[VMIN]=1;mode.c_cc[VTIME]=0;
        if(tcsetattr(input_fd,TCSANOW,&mode)!=0)goto failed;
        display_fd=input_fd;
    } else {
        if(!fd)return 0;
        char *end;
        long value=strtol(fd,&end,10);
        if(*end || value<=STDERR_FILENO || value>2147483647 || fcntl((int)value,F_GETFD)<0)
            return 0;
        display_fd=(int)value;
        if(fcntl(display_fd,F_SETFD,FD_CLOEXEC)<0)return 0;
    }
    bluetooth_remote=getenv("LUMEN_EMULATOR_BLUETOOTH")!=NULL;
    signal(SIGPIPE,SIG_IGN);
    return 1;
failed:
    close(input_fd);
    input_fd=STDIN_FILENO;
    return 0;
}
void virtual_device_close(void) {
    if(display_fd>STDERR_FILENO)close(display_fd);
}
void virtual_device_present(const unsigned char *pixels, int full, int interval) {
    (void)interval;
    present(pixels, full);
}
void virtual_device_present_async(const unsigned char *pixels,int full) { present(pixels,full); }
void virtual_device_poll_interval(int ms) { timeout_ms = ms; }
int virtual_device_refresh_busy(void) { return 0; }
int virtual_device_input_pending(void) {
    struct pollfd fd = {input_fd, POLLIN, 0};
    return poll(&fd, 1, 0) > 0 && (fd.revents & POLLIN);
}
int virtual_device_wait(Input *input) {
    *input = (Input){0};
    int64_t status_due = timeout_ms < 0 ? INT64_MAX : milliseconds() + timeout_ms;
    for (;;) {
        if (pending_sleep) {
            pending_sleep = 0;
            asleep = 1;
            input->kind = INPUT_SLEEP;
            return 1;
        }
        if (pending_power_release) {
            pending_power_release = 0;
            input->kind = INPUT_POWER;
            return 1;
        }
        int64_t now = milliseconds(), deadline = status_due;
        if (touching && !held && touch_started + 550 < deadline) deadline = touch_started + 550;
        if (power_pressed && !power_held && power_started + 900 < deadline)
            deadline = power_started + 900;
        struct pollfd fds[3+APP_OVERLAY_COUNT]={{input_fd,POLLIN,0},{app_process_fd(),POLLIN,0}};
        for(int i=0;i<APP_OVERLAY_COUNT;i++)fds[2+i]=(struct pollfd){app_overlay_fd(i),POLLIN,0};
        fds[2+APP_OVERLAY_COUNT]=(struct pollfd){app_service_fd(),POLLIN,0};
        int service_wait=app_service_wait_ms();
        if(service_wait>=0 && now+service_wait<deadline)deadline=now+service_wait;
        int ready = poll(fds, 3+APP_OVERLAY_COUNT, deadline == INT64_MAX ? -1 :
                        deadline > now ? (int)(deadline - now) : 0);
        if (ready < 0 && errno == EINTR) continue;
        if (ready < 0) break;
        if (!ready) {
            now = milliseconds();
            if (power_pressed && !power_held && now >= power_started + 900) {
                power_held = 1;
                *input = (Input){.kind = INPUT_POWER, .x = 2};
                return 1;
            }
            if (touching && !held && now >= touch_started + 550) {
                held = 1;
                *input = (Input){.kind = INPUT_HOLD, .x = touch_x, .y = touch_y};
                return 1;
            }
            if (!app_service_wait_ms()) {input->kind=INPUT_APP_SERVICE;return 1;}
            if (now >= status_due) {
                input->kind = INPUT_STATUS;
                return 1;
            }
            continue;
        }
        if(!(fds[0].revents&(POLLIN|POLLHUP))) {
            for(int i=0;i<APP_OVERLAY_COUNT;i++)if(fds[2+i].revents&(POLLIN|POLLHUP|POLLERR)) {
                input->kind=INPUT_OVERLAY;input->x=i;return 1;
            }
            if(fds[2+APP_OVERLAY_COUNT].revents&(POLLIN|POLLHUP|POLLERR)) {
                input->kind=INPUT_APP_SERVICE;return 1;
            }
            if(fds[1].revents&(POLLIN|POLLHUP|POLLERR)) {input->kind=INPUT_APP;return 1;}
            continue;
        }
        Packet packet;
        if (!read_packet(&packet) || packet.kind == STOP) break;
        if(packet.kind==BLUETOOTH_RESULT && bluetooth_remote) {
            if(packet.x>=0 && packet.x<=3)bluetooth_connection=packet.x;
            input->kind=INPUT_STATUS;return 1;
        }
        if (packet.kind == POWER_DOWN) {
            if (power_pressed) continue;
            power_pressed = 1;
            power_held = 0;
            power_started = milliseconds();
            *input = (Input){.kind = INPUT_POWER, .x = 1};
            return 1;
        }
        if (packet.kind == POWER_UP) {
            if (!power_pressed) continue;
            power_pressed = 0;
            if (power_held) input->kind = INPUT_POWER;
            else {
                asleep = !asleep;
                pending_power_release = 1;
                input->kind = asleep ? INPUT_SLEEP : INPUT_WAKE;
            }
            return 1;
        }
        if (packet.kind == TOUCH_DOWN) {
            touching = 1;
            held = 0;
            touch_started = milliseconds();
            touch_x = start_x = packet.x;
            touch_y = start_y = packet.y;
            *input = (Input){.kind = INPUT_DOWN, .x = touch_x, .y = touch_y};
            return 1;
        }
        if (!touching) continue;
        touch_x = packet.x;
        touch_y = packet.y;
        int dx = touch_x - start_x, dy = touch_y - start_y;
        if (packet.kind == TOUCH_MOVE) {
            if (abs(dx) > 18 || abs(dy) > 18) held = 1;
            *input = (Input){INPUT_MOVE, touch_x, touch_y, dx, dy};
            return 1;
        }
        touching = 0;
        if (packet.kind == TOUCH_CANCEL) {
            *input = (Input){.kind = INPUT_CANCEL};
            return 1;
        }
        if (packet.kind == TOUCH_UP) {
            *input = (Input){abs(dx) > 65 || abs(dy) > 80 ? INPUT_SWIPE : INPUT_TAP,
                             touch_x, touch_y, dx, dy};
            return 1;
        }
    }
    input->kind = INPUT_EXIT;
    return 1;
}
void virtual_device_status(char *battery, int size, char *wifi, int wifi_size) {
    snprintf(battery, size, "68");
    snprintf(wifi, wifi_size, "Offline");
}
int virtual_device_charging(void) { return 0; }
void virtual_device_activity(void) {}
void virtual_device_request_sleep(void) { if (!asleep) pending_sleep = 1; }
int virtual_device_wake(void) { asleep = pending_sleep = 0; return 1; }
int virtual_device_system_power(int restart) { (void)restart; return 1; }
DeviceCapabilities virtual_device_capabilities(void) { return (DeviceCapabilities){brightness,bluetooth}; }
int virtual_device_brightness(int percent) { brightness=percent;return 1; }
int virtual_device_bluetooth(int enabled) {
    if(!enabled && bluetooth_connection==1 && !virtual_device_bluetooth_connect(0))return 0;
    bluetooth=!!enabled;
    return 1;
}
int virtual_device_bluetooth_remote(void) {return bluetooth_remote;}
int virtual_device_bluetooth_connection(void) {return bluetooth_connection;}
int virtual_device_bluetooth_connect(int connect) {
    if(!bluetooth || bluetooth_connection==2)return 0;
    if(bluetooth_remote) {
        unsigned char request[]={128,!!connect};
        if(!write_all(request,sizeof(request)))return 0;
        bluetooth_connection=2;
    } else bluetooth_connection=!!connect;
    return 1;
}
int virtual_device_wifi_enabled(void) { return wifi_on; }
int virtual_device_wifi(int enabled) { wifi_on=!!enabled;return 1; }
void virtual_device_ip(char *address, int size) { snprintf(address, size, "127.0.0.1"); }
void virtual_device_hold_awake(int enabled) { (void)enabled; }
void virtual_device_finish_update(void) {}
