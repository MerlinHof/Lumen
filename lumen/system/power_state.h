#ifndef LUMEN_POWER_STATE_H
#define LUMEN_POWER_STATE_H
#include <string.h>

typedef enum { POWER_UNKNOWN, POWER_AWAKE, POWER_ASLEEP } PowerState;

/* KT2 powerd status begins with a named state line. Unknown outputs leave
 * the most recent complete transition in charge. */
static inline PowerState power_state_status(const char *text) {
    const char prefix[]="Powerd state: ";
    if(strncmp(text,prefix,sizeof(prefix)-1))return POWER_UNKNOWN;
    text+=sizeof(prefix)-1;
    if(!strcmp(text,"Active"))return POWER_AWAKE;
    if(!strcmp(text,"Screen Saver") || !strcmp(text,"ScreenSaver") ||
       !strcmp(text,"Ready to suspend") || !strcmp(text,"Suspended"))return POWER_ASLEEP;
    return POWER_UNKNOWN;
}
static inline PowerState power_state_event(const char *line) {
    size_t length=strcspn(line," \t\r");
    if(length==sizeof("goingToScreenSaver")-1 && !strncmp(line,"goingToScreenSaver",length))
        return POWER_ASLEEP;
    if((length==sizeof("outOfScreenSaver")-1 && !strncmp(line,"outOfScreenSaver",length)) ||
       (length==sizeof("exitingScreenSaver")-1 && !strncmp(line,"exitingScreenSaver",length)))
        return POWER_AWAKE;
    return POWER_UNKNOWN;
}
#endif
