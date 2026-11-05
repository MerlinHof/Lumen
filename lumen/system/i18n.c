#include "i18n.h"
#include "library.h"
#include <string.h>
#include "i18n_catalog.h"
size_t i18n_count(void) {
    return sizeof(translations) / sizeof(*translations);
}
const char *tr_language(const char *text, int language) {
    if (!text)
        return "";
    if (language < 0 || language > 2)
        language = 0;
    size_t lo = 0, hi = i18n_count();
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        int c = strcmp(text, translations[mid].key);
        if (c > 0)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo < i18n_count() && !strcmp(text, translations[lo].key)
               ? translations[lo].values[language]
               : text;
}
const char *tr(const char *text) {
    return tr_language(text, options[OPT_LANGUAGE]);
}
