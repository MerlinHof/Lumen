#ifndef LUMEN_APP_PACKAGES_H
#define LUMEN_APP_PACKAGES_H
#include "app_runtime.h"
#define APP_PACKAGE_API 1
#define APP_FRAME_MAX 262144
void app_packages_scan(void);
void app_packages_seed(void);
const AppDefinition *app_package_definition(AppId id);
int app_package_installed(AppId id);
const char *app_package_key(AppId id);
const char *app_package_version(AppId id);
int app_package_install(const char *archive);
int app_package_remove(const char *id);
int app_package_deleted(AppId id);
int app_package_icon(AppId id,unsigned char pixels[76*76]);
int app_process_open(AppId id);
void app_process_pause(void);
void app_process_close(void);
int app_process_poll(void);
int app_process_fd(void);
int app_process_pending(void);
int app_process_send(const char *event);
const char *app_process_frame(void);
const char *app_process_request(void);
const char *app_process_error(void);
#endif
