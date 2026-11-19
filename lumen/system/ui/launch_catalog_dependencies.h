#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
static int v1_ready, v1_dirty;
#include "media_state.h"
#include "launch_catalog_state.h"
#include "alarm_flash_state.h"
static void media_scan(void);
