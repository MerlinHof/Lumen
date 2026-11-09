#ifndef LUMEN_RELEASE_UPDATE_H
#define LUMEN_RELEASE_UPDATE_H
#include <time.h>
/* Full verified runtime packages from GitHub, with durable rollback. */
int release_start(int install);
int release_poll(void);
int release_progress(int *mb);
int release_activate(void);
void release_cancel(void);
int release_state(void); /* 0 not checked, 1 busy, 2 current, 3 available, -1 failed */
const char *release_version(void);
const char *release_error(void);
time_t release_checked(void);
#endif
