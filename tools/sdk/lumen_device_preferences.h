#ifndef LUMEN_DEVICE_PREFERENCES_H
#define LUMEN_DEVICE_PREFERENCES_H

/* Public preference names. Keep the original order for Protocol 1 replies. */
#define LUMEN_DEVICE_PREFERENCES(X) \
    X("language", LANGUAGE) \
    X("keyboard_language", KEYBOARD_LANGUAGE) \
    X("invert", INVERT) \
    X("rounding", ROUNDING) \
    X("show_date", DATE) \
    X("show_clock", CLOCK) \
    X("show_wifi", WIFI) \
    X("show_battery", BATTERY) \
    X("battery_percent", BATTERY_PERCENT) \
    X("lock_mode", LOCK) \
    X("lock_time", LOCK_TIME) \
    X("lock_date", LOCK_DATE) \
    X("lock_hint", LOCK_HINT) \
    X("auto_time", AUTO_TIME) \
    X("time_zone", ZONE)

#endif
