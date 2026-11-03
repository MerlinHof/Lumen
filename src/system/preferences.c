#include "preferences.h"
#include "../library.h"
ReaderPreferences preferences_reader(void) {
    return (ReaderPreferences){settings.size,         settings.leading,   settings.margin,
                               options[OPT_FONT],     options[OPT_ALIGN], options[OPT_LETTERSPACE],
                               options[OPT_PARAGRAPH]};
}
int preferences_keyboard_language(void) {
    int lang = options[OPT_KEYBOARD_LANGUAGE];
    return lang >= 0 && lang < 3 ? lang : 0;
}
