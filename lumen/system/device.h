#ifndef LUMEN_DEVICE_H
#define LUMEN_DEVICE_H
#include "../../tools/sdk/lumen_input.h"
int device_init(void);
void device_close(void);
int device_wait(Input *input);
int device_input_pending(void);
int device_refresh_busy(void);
void device_present_async(const unsigned char *pixels);
void device_present(const unsigned char *pixels, int full, int interval);
void device_status(char *battery, int cap, char *wifi, int nc);
int device_charging(void);
void device_activity(void);
int device_wake(void);
void device_request_sleep(void);
int device_system_power(int restart);
void device_poll_interval(int ms);
int device_wifi(int enabled);
void device_hold_awake(int enabled);
void device_finish_update(void);
int device_wifi_enabled(void);
void device_ip(char *out, int size);
#endif
