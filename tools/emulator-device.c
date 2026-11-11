/* Host-only virtual hardware for the browser emulator. */
#define _POSIX_C_SOURCE 200809L
#include "system/device.h"
#include "system/refresh.h"
#include <string.h>
#include <errno.h>
#include <poll.h>
#include "../lumen/system/app_packages.h"
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

enum { TOUCH_DOWN = 1, TOUCH_MOVE, TOUCH_UP, TOUCH_CANCEL, POWER_DOWN, POWER_UP, STOP };
typedef struct { int32_t kind, x, y; } Packet;
static int display_fd, timeout_ms = -1, asleep, pending_sleep, pending_power_release;
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
        ssize_t n = read(STDIN_FILENO, destination, remaining);
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
    int top = HEIGHT, bottom = 0, left = WIDTH, right = 0;
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            int at = y*WIDTH+x;
            if (initialized && previous[at] == pixels[at]) continue;
            if (x < left) left = x;
            if (x >= right) right = x+1;
            if (y < top) top = y;
            if (y >= bottom) bottom = y+1;
        }
    }
    if (top == HEIGHT && !full) return;
    /* Match the device's refresh classification, including its dirty bounds. */
    int area = (right-left)*(bottom-top);
    int clear = !initialized || (top < HEIGHT && area >= WIDTH*HEIGHT*60/100 &&
                refresh_coverage(previous, pixels, WIDTH, HEIGHT) >= 70);
    if (top < STATUS_HEIGHT || full) top = 0;
    if (top > 0) {
        left &= ~7;
        right = (right+7) & ~7;
        top &= ~1;
        bottom = (bottom+1) & ~1;
    }
    int binary = top >= STATUS_HEIGHT && (right-left)*(bottom-top) < WIDTH*HEIGHT/4;
    for (int y = top; y < bottom && binary; y++)
        for (int x = left; x < right; x++) {
            int value = pixels[y*WIDTH+x];
            int old = previous[y*WIDTH+x];
            if ((value != 0 && value != 255) || (old != 0 && old != 255)) { binary = 0; break; }
        }
    unsigned char mode = clear ? 3 : top == 0 ? 2 : binary ? 0 : 1;
    if (write_all(&mode, 1) && write_all(pixels, sizeof(previous))) {
        memcpy(previous, pixels, sizeof(previous));
        initialized = 1;
    }
}
int device_init(void) {
    const char *fd = getenv("LUMEN_EMULATOR_DISPLAY_FD");
    if (!fd) return 0;
    display_fd = atoi(fd);
    signal(SIGPIPE, SIG_IGN);
    return display_fd > STDERR_FILENO;
}
void device_close(void) { if (display_fd > STDERR_FILENO) close(display_fd); }
void device_present(const unsigned char *pixels, int full, int interval) {
    (void)interval;
    present(pixels, full);
}
void device_present_async(const unsigned char *pixels) { present(pixels, 0); }
void device_poll_interval(int ms) { timeout_ms = ms; }
int device_refresh_busy(void) { return 0; }
int device_input_pending(void) {
    struct pollfd fd = {STDIN_FILENO, POLLIN, 0};
    return poll(&fd, 1, 0) > 0 && (fd.revents & POLLIN);
}
int device_wait(Input *input) {
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
        struct pollfd fds[2+APP_OVERLAY_COUNT]={{STDIN_FILENO,POLLIN,0},{app_process_fd(),POLLIN,0}};
        for(int i=0;i<APP_OVERLAY_COUNT;i++)fds[2+i]=(struct pollfd){app_overlay_fd(i),POLLIN,0};
        int ready = poll(fds, 2+APP_OVERLAY_COUNT, deadline == INT64_MAX ? -1 :
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
            if(fds[1].revents&(POLLIN|POLLHUP|POLLERR)) {input->kind=INPUT_APP;return 1;}
            continue;
        }
        Packet packet;
        if (!read_packet(&packet) || packet.kind == STOP) break;
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
void device_status(char *battery, int size, char *wifi, int wifi_size) {
    snprintf(battery, size, "68");
    snprintf(wifi, wifi_size, "Offline");
}
int device_charging(void) { return 0; }
void device_activity(void) {}
void device_request_sleep(void) { if (!asleep) pending_sleep = 1; }
int device_wake(void) { asleep = pending_sleep = 0; return 1; }
int device_system_power(int restart) { (void)restart; return 1; }
int device_wifi_enabled(void) { return 0; }
int device_wifi(int enabled) { (void)enabled; return 0; }
void device_ip(char *address, int size) { snprintf(address, size, "127.0.0.1"); }
void device_hold_awake(int enabled) { (void)enabled; }
void device_finish_update(void) {}
