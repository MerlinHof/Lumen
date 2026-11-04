/* App-owned module; assembled by apps/host/compose.h. */
static void flash_next(void) {
    flash_selected = -1;
    flash_reveal = 0;
    int64_t now = time(NULL);
    for (int i = 0; i < flash_count; i++)
        if (flashcards[i].memory.due <= now &&
            (flash_selected < 0 ||
             flashcards[i].memory.due < flashcards[flash_selected].memory.due))
            flash_selected = i;
}
static void v1_flash_draw(void) {
    heading_actions("Karteikarten", 2);
    icon_button(528, 74, IC_PLUS);
    icon_button(468, 74, IC_CONTENTS);
    int due = 0;
    for (int i = 0; i < flash_count; i++)
        due += flashcards[i].memory.due <= time(NULL);
    char b[80];
    snprintf(b, 80, "%d · %d %s · FSRS-6", flash_count, due, tr("Fällig"));
    centered(28, 136, 544, 36, b, ui_font(UI_SECONDARY));
    if (flash_selected < 0) {
        centered(28, 280, 544, 70, "Für heute geschafft.", ui_font(UI_TITLE));
        wrapped(40, 385,
                flash_count
                    ? "Die nächste Wiederholung erscheint automatisch, sobald sie fällig ist."
                    : "Mit + eine Karte erstellen. Beliebiges Thema, eigene Frage und Antwort.",
                ui_font(UI_PRIMARY), 520, 4);
        return;
    }
    Flashcard *c = &flashcards[flash_selected];
    drawtext_raw(32, 215, c->deck, ui_font(UI_SECONDARY), 536);
    wrapped_raw(32, 275, c->front, ui_font(UI_TITLE), 536, 5);
    if (!flash_reveal)
        button_h(28, 622, 544, 60, "Antwort zeigen", 0);
    else {
        divider(28, 440, 544);
        wrapped_raw(32, 470, c->back, ui_font(UI_BODY), 536, 6);
        const char *ratings[] = {"Nochmal", "Schwer", "Gut", "Leicht"};
        for (int i = 0; i < 4; i++) {
            MemoryCard m = c->memory;
            fsrs_review(&m, i + 1, time(NULL));
            long secs = m.due - time(NULL);
            snprintf(b, 80,
                     secs < 3600    ? "%ld min"
                     : secs < 86400 ? "%ld h"
                                    : "%ld d",
                     secs < 3600    ? secs / 60
                     : secs < 86400 ? secs / 3600
                                    : secs / 86400);
            centered(28 + i * 140, 687, 124, 30, b, ui_font(UI_SECONDARY));
            button_h(28 + i * 140, 722, 124, 54, ratings[i], 0);
        }
    }
}
