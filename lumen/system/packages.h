#ifndef LUMEN_PACKAGES_H
#define LUMEN_PACKAGES_H
#include <stddef.h>
#define PACKAGE_FILES 192
/* All paths are relative, regular files; extraction never follows symlinks. */
typedef struct { char path[256],sha256[65];size_t size; } PackageFile;
typedef struct {
 char format[32],version[32],id[64],name[96];int api,count;
 PackageFile files[PACKAGE_FILES];
} Package;
int package_manifest(const char *text,Package *p);
int package_path_valid(const char *path);
int package_extract(const char *archive,const char *destination,Package *p);
int package_sha256(const char *path,char digest[65]);
int package_mkdirs(const char *path);
int package_remove(const char *path);
int package_copy(const char *source,const char *target);
int package_peek(const char *archive,Package *p);
#endif
