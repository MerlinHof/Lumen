#ifndef LUMEN_SCRATCH_H
#define LUMEN_SCRATCH_H
#define SCRATCH_LINES 128
typedef struct {
    int start, length, state;
    char result[96];
} ScratchLine;
int scratch_evaluate(const char *text, ScratchLine *lines, int capacity);
#endif
