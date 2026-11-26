#ifndef LUMEN_VIRTUAL_DEVICE_H
#define LUMEN_VIRTUAL_DEVICE_H
#include "device.h"
#ifndef LUMEN_HOST
int virtual_device_guest_bootstrap(void);
#endif
int virtual_device_init(void);
void virtual_device_close(void);
int virtual_device_wait(Input *input);
int virtual_device_input_pending(void);
int virtual_device_refresh_busy(void);
void virtual_device_present_async(const unsigned char *pixels,int full);
void virtual_device_present(const unsigned char *pixels, int full, int interval);
void virtual_device_status(char *battery, int cap, char *wifi, int nc);
int virtual_device_charging(void);
void virtual_device_activity(void);
int virtual_device_wake(void);
void virtual_device_request_sleep(void);
int virtual_device_system_power(int restart);
void virtual_device_poll_interval(int ms);
int virtual_device_wifi(int enabled);
DeviceCapabilities virtual_device_capabilities(void);
int virtual_device_brightness(int percent);
int virtual_device_bluetooth(int enabled);
int virtual_device_bluetooth_remote(void);
int virtual_device_bluetooth_connection(void);
int virtual_device_bluetooth_connect(int connect);
void virtual_device_hold_awake(int enabled);
void virtual_device_finish_update(void);
int virtual_device_wifi_enabled(void);
void virtual_device_ip(char *out, int size);
#endif
