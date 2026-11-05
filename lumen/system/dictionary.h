#ifndef LUMEN_DICTIONARY_H
#define LUMEN_DICTIONARY_H
#include <stddef.h>
int dictionary_translate(const char *, int, char *, size_t);
const char *dictionary_lookup(const char *word, int preferred, int *language);
void dictionary_game_word(int language, unsigned random, char out[64]);
#endif
