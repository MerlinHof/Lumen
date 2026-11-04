/* App-owned state; stable on-disk schemas are intentionally preserved. */
typedef struct {
    uint64_t id;
    char deck[64], front[512], back[1024];
    MemoryCard memory;
} Flashcard;
static Flashcard flashcards[512];
static int flash_count, flash_selected = -1, flash_reveal, flash_page, flash_edit = -1;
static char flash_deck[64] = "General", flash_front[512];
