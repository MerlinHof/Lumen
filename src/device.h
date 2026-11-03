#ifndef LUMEN_DEVICE_H
#define LUMEN_DEVICE_H
#define WIDTH 600
#define HEIGHT 800
typedef struct {
    int kind, x, y, dx, dy;
} Input;
enum {
    INPUT_NONE,
    INPUT_TAP,
    INPUT_SWIPE,
    INPUT_SLEEP,
    INPUT_WAKE,
    INPUT_USB,
    INPUT_EXIT,
    INPUT_DOWN,
    INPUT_MOVE,
    INPUT_NETWORK,
    INPUT_STATUS,
    INPUT_POWER,
    INPUT_HOLD
};
int device_init(void);
void device_close(void);
int device_wait(Input *input);
int device_input_pending(void);
void device_present_feedback(const unsigned char *pixels);
void device_present(const unsigned char *pixels, int full, int interval);
void device_status(char *battery, int cap, char *wifi, int nc);
int device_charging(void);
void device_activity(void);
int device_wake(void);
void device_request_sleep(void);
int device_system_power(int restart);
void device_poll_interval(int ms);
int device_wifi(int enabled);
int device_wifi_enabled(void);
void device_ip(char *out, int size);
#endif
