#include "../src/engine.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static unsigned long hash(unsigned char *p) {
    unsigned long h = 5381;
    for (int i = 0; i < 544 * 728; i++)
        h = (h * 33) ^ p[i];
    return h;
}
int main(int argc, char **argv) {
    assert(argc == 2);
    assert(engine_init());
    unsigned char *pixels = malloc(544 * 728);
    assert(pixels);
    engine_options(0, 0, 0, 8);
    assert(engine_open(argv[1], 18, 120, 0));
    assert(engine_reflowable());
    int count = engine_count();
    assert(engine_render(1, pixels, 544, 544, 728, 1, 0, 0));
    unsigned long a = hash(pixels);
    TextSelection found, chosen;
    /* Use a real text hit to choose coordinates rather than assuming a font's metrics. */
    assert(engine_find_text(1, "Lesen", 544, 728, 1, 0, 0, &found));
    assert(found.count > 0);
    TextRect r = found.rects[0];
    assert(engine_select(1, (r.x0 + r.x1) / 2, (r.y0 + r.y1) / 2, (r.x0 + r.x1) / 2 + 1,
                         (r.y0 + r.y1) / 2 + 1, 544, 728, 1, 0, 0, &chosen));
    assert(strstr(chosen.text, "Lesen") || strstr(chosen.text, "lesen"));
    assert(chosen.count > 0);
    ReaderPosition position;
    assert(engine_position(1, &position));
    snprintf(position.quote, sizeof(position.quote), "%s", chosen.text);
    engine_options(2, 1, 2, 16);
    int page = engine_relayout(32, 180, 28, 1);
    assert(engine_count() > count);
    assert(engine_render(page, pixels, 544, 544, 728, 1, 0, 0));
    assert(hash(pixels) != a);
    int restored = engine_restore(&position);
    assert(restored >= 0 && restored < engine_count());
    unsigned long fonts[5];
    for (int font = 0; font < 5; font++) {
        engine_options(font, 0, 0, 8);
        assert(engine_open(argv[1], 24, 145, 12));
        assert(engine_render(1, pixels, 544, 544, 728, 1, 0, 0));
        fonts[font] = hash(pixels);
        for (int old = 0; old < font; old++)
            assert(fonts[font] != fonts[old]);
    }
    engine_shutdown();
    free(pixels);
    puts("PASS: publisher CSS overridden, font/size/margin reflow, text selection, highlight "
         "rectangles and anchor restoration.");
}
