static int64_t extra_millis(void);
#include "alarm_state.h"
static int reading_goal=20; /* Previous combined settings file; imported by Reading Statistics. */
#include "time_sync_state.h"
/* Preserved field in the pre-package settings schema. */
static int legacy_clock_style;
