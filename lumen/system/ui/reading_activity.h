/* Shared reading activity records used by Reader and package clients. */
#include <ctype.h>
#include <dirent.h>
#include <sys/time.h>
typedef struct {
    uint64_t id;
    unsigned seconds, pages, sessions;
} BookInsight;
typedef struct {
    unsigned magic, sessions;
    BookInsight books[BOOK_LIMIT];
} Insights;
static Insights insights;
static int insights_dirty;
static time_t insight_last;
static uint64_t insight_book;
static BookInsight *insight_get(uint64_t id, int create) {
    if (!id)
        return NULL;
    BookInsight *empty = NULL;
    for (int i = 0; i < BOOK_LIMIT; i++) {
        if (insights.books[i].id == id)
            return &insights.books[i];
        if (!insights.books[i].id && !empty)
            empty = &insights.books[i];
    }
    if (create && empty) {
        empty->id = id;
        return empty;
    }
    return NULL;
}
static void extra_track(unsigned seconds, unsigned pages, uint64_t id) {
    BookInsight *b = insight_get(id, 1);
    if (!b)
        return;
    time_t now = time(NULL);
    if (insight_book != id || now - insight_last > 600) {
        insights.sessions++;
        b->sessions++;
    }
    insight_book = id;
    insight_last = now;
    b->seconds += seconds;
    b->pages += pages;
    insights_dirty = 1;
}
