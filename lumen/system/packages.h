#ifndef LUMEN_PACKAGES_H
#define LUMEN_PACKAGES_H
#include <stddef.h>
#define PACKAGE_FILES 192
#define PACKAGE_OPERATIONS 16
#define PACKAGE_CONFIGURATION 8

enum { PACKAGE_FOREGROUND=1, PACKAGE_OVERLAY=2, PACKAGE_BACKGROUND=4 };
typedef enum {
    PROVIDER_STARTUP, PROVIDER_TEXT_INPUT, PROVIDER_STATUS,
    PROVIDER_NOTIFICATION, PROVIDER_SLEEP, PROVIDER_DOCUMENT, PROVIDER_BOOKS,
    PROVIDER_DEVICE_CONTROL, PROVIDER_FILE_TRANSFER, PROVIDER_ALARM,
    PROVIDER_AGENDA, PROVIDER_WRITTEN_DOCUMENTS, PROVIDER_COUNT
} PackageProvider;
extern const char *const package_provider_names[PROVIDER_COUNT];
/* All paths are relative, regular files; extraction never follows symlinks. */
typedef struct { char path[256],sha256[65];size_t size; } PackageFile;
typedef struct {
 char format[32],version[32],id[64],name[96];int api,count;
 unsigned surfaces, providers;
 int launcher_order,launcher_visible;
 int operation_count,configuration_count,periodic_interval;
 char periodic_operation[48];
 char configuration[PACKAGE_CONFIGURATION][64];
 char operations[PACKAGE_OPERATIONS][48];
 PackageFile files[PACKAGE_FILES];
} Package;
int package_configuration_valid(const char *name);
int package_operation_valid(const char *operation);
int package_manifest(const char *text,Package *p);
int package_path_valid(const char *path);
int package_extract(const char *archive,const char *destination,Package *p);
int package_sha256(const char *path,char digest[65]);
int package_mkdirs(const char *path);
int package_remove(const char *path);
int package_copy(const char *source,const char *target);
int package_peek(const char *archive,Package *p);
#endif
