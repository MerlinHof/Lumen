#ifndef LUMEN_APP_PACKAGES_H
#define LUMEN_APP_PACKAGES_H
#include "app_runtime.h"
#include "packages.h"
#define APP_PACKAGE_API 2
#define APP_FRAME_MAX 262144
void app_packages_scan(void);
void app_packages_seed(void);
int app_package_installed(AppId id);
int app_package_foreground(AppId id);
int app_package_launcher_order(AppId id);
int app_package_launcher_visible(AppId id);
const char *app_legacy_key(int slot);
int app_package_find(const char *key);
const char *app_factory_provider(PackageProvider role);
const char *app_package_provider(PackageProvider role);
int app_package_provider_id(PackageProvider role);
int app_package_asset(AppId id,const char *name,char *path,size_t size);
const char *app_package_key(AppId id);
const char *app_package_version(AppId id);
int app_package_install(const char *archive);
int app_package_configure(const char *id,const char *key,const char *bytes,size_t length);
int app_package_remove(const char *id);
int app_package_can_remove(AppId id);
int app_package_deleted(AppId id);
int app_package_icon(AppId id,unsigned char pixels[76*76]);
int app_package_export(const char *id,const char *operation,const char *data,
                       const char *output,char *result,size_t capacity);
int app_service_start(const char *id,const char *operation,const char *data,int action);
int app_service_start_background(const char *id,const char *operation,const char *data,int action);
int app_service_fd(void);
int app_service_wait_ms(void);
void app_service_poll(void);
void app_periodic_poll(int awake);
void app_system_query_poll(void);
int app_system_request(PackageProvider role,const char *operation,void (*complete)(const char *));
int app_process_open(AppId id);
void app_process_pause(void);
void app_process_close(void);
void app_process_detach_inherited(void);
int app_process_poll(void);
int app_process_owner(void);
const char *app_process_data_dir(void);
const char *app_process_render_dir(void);
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
enum { APP_OVERLAY_COUNT=8 };
int app_overlay_open(const char *id,const char *event);
int app_provider_overlay_slot(PackageProvider role);
PackageProvider app_overlay_role(int slot);
int app_provider_overlay_send(PackageProvider role,const char *event);
void app_overlay_close(int slot);
int app_overlay_event(int slot,const char *event);
int app_overlay_fd(int slot);
int app_overlay_poll(int slot);
const char *app_overlay_frame(int slot);
uint64_t app_overlay_frame_serial(int slot);
#endif
