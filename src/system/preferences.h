#ifndef LUMEN_PREFERENCES_H
#define LUMEN_PREFERENCES_H
/* Value snapshots: apps never retain pointers into the settings UI. */
typedef struct {
    int size, leading, margin, font, alignment, letterspace, paragraph;
} ReaderPreferences;
ReaderPreferences preferences_reader(void);
int preferences_keyboard_language(void);
#endif
