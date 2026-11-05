/* App-owned state; stable on-disk schemas are intentionally preserved. */
static struct {
    int date, row, over;
    char guesses[6][6];
    unsigned char result[6][5];
} wordle_game;
static char word_secret[6];
