#ifndef LUMEN_APP_STORAGE_H
#define LUMEN_APP_STORAGE_H
#include <stddef.h>
int app_state_load(const char *name, void *data, size_t bytes);
int app_state_save(const char *name, const void *data, size_t bytes);
#endif
