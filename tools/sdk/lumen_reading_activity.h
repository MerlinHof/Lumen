#ifndef LUMEN_READING_ACTIVITY_H
#define LUMEN_READING_ACTIVITY_H

/* Existing on-device reading activity snapshot shared by Reader clients. */
#define LUMEN_READING_ACTIVITY_FILE "apps-v1.bin"
#define LUMEN_READING_ACTIVITY_MAGIC 0x4c415050
typedef struct {
    int date;
    unsigned seconds, pages;
} ReadingDay;
typedef struct {
    unsigned magic, version, seconds, pages, best_snake, best_blocks, mines_wins, word_wins;
    ReadingDay days[31];
} ReadingActivityV1;
#endif
