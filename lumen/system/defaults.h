#ifndef LUMEN_DEFAULTS_H
#define LUMEN_DEFAULTS_H
#include "library.h"
#define LUMEN_DEFAULT_SETTINGS {24, 145, 12, 8, 1, 2, 0}
#define LUMEN_DEFAULT_OPTIONS { \
    [OPT_FONT] = 0, \
    [OPT_ALIGN] = 0, \
    [OPT_COVERS] = 1, \
    [OPT_AUTHORS] = 1, \
    [OPT_CLOCK] = 1, \
    [OPT_BATTERY] = 1, \
    [OPT_WIFI] = 1, \
    [OPT_INVERT] = 0, \
    [OPT_SWIPE] = 1, \
    [OPT_TAP] = 0, \
    [OPT_RESUME] = 0, \
    [OPT_LOCK] = 0, \
    [OPT_PORT] = 8080, \
    [OPT_TIMEOUT] = 10, \
    [OPT_AUTOSTART] = 1, \
    [OPT_CONFIRM] = 1, \
    [OPT_LETTERSPACE] = 0, \
    [OPT_PARAGRAPH] = 8, \
    [OPT_STARTPAGE] = 0, \
    [OPT_LANGUAGE] = 0, \
    [OPT_LOCK_HINT] = 0, \
    [OPT_DATE] = 0, \
    [OPT_BATTERY_PERCENT] = 0, \
    [OPT_COVER_CROP] = 1, \
    [OPT_ROUNDING] = 1, \
    [OPT_PROGRESS] = 1, \
    [OPT_APPS] = 1, \
    [OPT_UI_REV] = 11, \
    [OPT_TAB_LABELS] = 0, \
    [OPT_AUTO_TIME] = 1, \
    [OPT_ZONE] = 0, \
    [OPT_REMOTE] = 0, \
    [OPT_TAB_BAR] = 1, \
    [OPT_KEYBOARD_LANGUAGE] = 0, \
}
extern const Settings factory_settings;
extern const int factory_options[OPT_COUNT], option_maximum[OPT_COUNT];
#endif
