#ifndef LUMEN_I18N_H
#define LUMEN_I18N_H
#include <stddef.h>
const char *tr(const char *text);
const char *tr_language(const char *text, int language);
size_t i18n_count(void);
#endif
