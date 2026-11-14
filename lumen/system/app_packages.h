#ifndef LUMEN_APP_PACKAGES_H
#define LUMEN_APP_PACKAGES_H
#include "app_runtime.h"
#define APP_PACKAGE_API 1
#define APP_FRAME_MAX 262144
void app_packages_scan(void);
void app_packages_seed(void);
const AppDefinition *app_package_definition(AppId id);
int app_package_installed(AppId id);
int app_package_find(const char *key);
int app_package_asset(AppId id,const char *name,char *path,size_t size);
const char *app_package_key(AppId id);
const char *app_package_version(AppId id);
int app_package_install(const char *archive);
int app_package_remove(const char *id);
int app_package_can_remove(AppId id);
int app_package_deleted(AppId id);
int app_package_icon(AppId id,unsigned char pixels[76*76]);
int app_service_start(const char *id,const char *operation,const char *data,int action);
int app_service_start_background(const char *id,const char *operation,const char *data,int action);
int app_service_fd(void);
int app_service_wait_ms(void);
void app_service_poll(void);
int app_process_open(AppId id);
void app_process_pause(void);
void app_process_close(void);
int app_process_poll(void);
int app_process_owner(void);
const char *app_process_data_dir(void);
int app_process_fd(void);
int app_process_pending(void);
void app_process_flush(void);
int app_process_send(const char *event);
int app_process_frame_back(void);
int app_process_frame_ready(void);
int app_process_frame_tick(void);
int app_process_frame_loading(void);
const char *app_process_frame(void);
uint64_t app_process_frame_serial(void);
const char *app_process_request(void);
const char *app_process_error(void);
enum { APP_OVERLAY_NOTIFICATION, APP_OVERLAY_STATUS, APP_OVERLAY_KEYBOARD, APP_OVERLAY_SLEEP, APP_OVERLAY_COUNT };
int app_overlay_send(int slot,const char *id,const char *event);
int app_overlay_fd(int slot);
int app_overlay_poll(int slot);
const char *app_overlay_frame(int slot);
uint64_t app_overlay_frame_serial(int slot);
#endif
