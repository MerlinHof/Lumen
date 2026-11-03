#define _GNU_SOURCE
#include "../src/system/ui/ui.c"
#include <assert.h>
#include <sys/stat.h>
#include <unistd.h>
int main(int argc, char **argv) {
    assert(argc == 3);
    library_root = argv[1];
    data_root = argv[2];
    assert(engine_init());
    assert(library_scan() >= 3);
    library_settings_load();
    ui_init();
    char folded[128];
    utf8_fold("ÄÖÜ ß Bücher", folded, sizeof(folded));
    assert(!strcmp(folded, "aou ss bucher"));
    utf8_fold("A\xcc\x88O\xcc\x88U\xcc\x88", folded, sizeof(folded));
    assert(!strcmp(folded, "aou"));
    memset(ui_pixels, 255, sizeof(ui_pixels));
    drawtext(0, 0, "ÄÖÜ äöü", &body, 500);
    unsigned char nfc[600 * 50];
    memcpy(nfc, ui_pixels, sizeof(nfc));
    memset(ui_pixels, 255, sizeof(ui_pixels));
    drawtext(0, 0, "A\xcc\x88O\xcc\x88U\xcc\x88 a\xcc\x88o\xcc\x88u\xcc\x88", &body, 500);
    assert(!memcmp(nfc, ui_pixels, sizeof(nfc)));
    int found[BOOK_LIMIT], n = library_view(found, "apfel", 0, 0);
    assert(n == 1);
    int epub = found[0];
    assert(!strcmp(library[epub].author, "LUMEN"));
    assert(ui_open(epub));
    assert(engine_count() > 3);
    assert(chapter_count == 3);
    assert(chapters[1].page > chapters[0].page);
    turn(1);
    assert(page == 1);
    int pages_before = engine_count();
    int newpage = engine_relayout(30, 160, 15, page);
    assert(newpage >= 0 && newpage < engine_count());
    assert(engine_count() > pages_before);
    page = newpage;
    library_progress(&library[epub], page, engine_count());
    unsigned char pix[544 * 580];
    assert(engine_render(0, pix, 544, 544, 580, 1, 0, 0));
    int ink = 0;
    for (size_t i = 0; i < sizeof(pix); i++)
        if (pix[i] < 128)
            ink++;
    assert(ink > 100);
    toggle_bookmark();
    assert(library[epub].bookmark_count == 1);
    toggle_bookmark();
    assert(library[epub].bookmark_count == 0);
    int before = library[epub].page;
    selected = 0;
    ui_save();
    assert(library[epub].page == page);
    (void)before;
    engine_close();
    current = -1;
    library[epub].favorite = 1;
    library_save_book(&library[epub]);
    assert(library_view(found, "", 4, 0) == 1);
    int saved = page;
    assert(library_scan() >= 3);
    n = library_view(found, "apfel", 0, 0);
    assert(n == 1);
    epub = found[0];
    assert(library[epub].favorite);
    assert(library[epub].page == saved);
    assert(library_view(found, "LUMEN", 3, 0) == 1);
    int pdf = found[0];
    assert(ui_open(pdf));
    assert(engine_count() == 3);
    assert(!engine_reflowable());
    assert(engine_render(1, pix, 544, 544, 580, 2, 100, -100));
    char broken[2048];
    snprintf(broken, sizeof(broken), "%s/broken.epub", library_root);
    assert(!engine_open(broken, 22, 145, 12));
    assert(*engine_error());
    assert(engine_open(library[pdf].path, 22, 145, 12));
    engine_shutdown();
    puts("PASS: Unicode/NFD, metadata, EPUB/PDF, TOC, reflow, zoom, per-book progress, favorites, "
         "bookmarks, cache reload and malformed-file recovery.");
    return 0;
}
