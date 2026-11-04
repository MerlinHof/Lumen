#ifndef LUMEN_LEARNING_H
#define LUMEN_LEARNING_H
#include <stdint.h>
typedef struct {
    double stability, difficulty;
    int64_t last, due;
    unsigned reviews, lapses;
} MemoryCard;
void fsrs_review(MemoryCard *c, int grade, int64_t now);
double fsrs_retrievability(const MemoryCard *c, int64_t now);
void wordle_score(const char *secret, const char *guess, unsigned char result[5]);
int hex_winner(const unsigned char *board, int n);
#endif
