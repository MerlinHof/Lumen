#define _GNU_SOURCE
#include "../src/system/ui/ui.c"
#include <assert.h>
static void tap(int x, int y) {
    Input e = {.kind = INPUT_TAP, .x = x, .y = y};
    ui_input(&e);
    ui_draw();
}
static void event(int kind, int x, int y) {
    Input e = {.kind = kind, .x = x, .y = y};
    ui_input(&e);
    ui_draw();
}
int main(int argc, char **argv) {
    assert(argc == 3);
    library_root = argv[1];
    data_root = argv[2];
    assert(engine_init());
    library_scan();
    ui_init();
    assert(options[OPT_APPS] && !options[OPT_TAB_LABELS]);
    strcpy(calc_expr, "2+4");
    calc_cursor = 2;
    calc_insert("3");
    assert(!strcmp(calc_expr, "2+34") && !strcmp(calc_result, "36"));
    calc_backspace();
    assert(!strcmp(calc_expr, "2+4"));
    calc_run();
    assert(calc_answer == 6);
    calc_insert("/0");
    calc_run();
    assert(*calc_error);
    calc_expr[0] = 0;
    calc_cursor = 0;
    calc_insert("sqrt(9)+sin(30)");
    calc_degrees = 1;
    calc_run();
    assert(fabs(calc_answer - 3.5) < 1e-12);
    screen = 27;
    ui_draw();
    assert(widget_count > 30);
    int books[BOOK_LIMIT];
    int n = library_view(books, "apfel", 0, 0);
    assert(n == 1 && ui_open(books[0]));
    int original_page = page;
    timer_due = extra_millis() - 1;
    Input tick = {.kind = INPUT_STATUS};
    assert(ui_input(&tick) == 1 && alarm_ring == 1 && !timer_due);
    ui_draw();
    assert(widget_count == 1);
    event(INPUT_DOWN, 300, 645);
    assert(ui_pixels[640 * 600 + 100] < 128);
    tap(300, 645);
    assert(!alarm_ring && screen == 2 && page == original_page);
    open_keyboard(30, "Notiz", "unterbrochen");
    timer_due = extra_millis() - 1;
    ui_input(&tick);
    ui_draw();
    assert(alarm_ring && keyboard);
    tap(300, 645);
    assert(keyboard && !strcmp(edit, "unterbrochen"));
    keyboard = 0;
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    alarm_hour = tm.tm_hour;
    alarm_minute = tm.tm_min;
    alarm_enabled = 1;
    alarm_last = 0;
    ui_input(&tick);
    assert(alarm_ring == 2 && !alarm_enabled);
    tap(300, 645);
    timer_due = extra_millis() + 60000;
    assert(ui_poll_interval() >= 0);
    Input sleep = {.kind = INPUT_SLEEP}, wake = {.kind = INPUT_WAKE};
    ui_input(&sleep);
    assert(sleeping && ui_poll_interval() == -1);
    ui_input(&wake);
    timer_due = extra_millis() + 60000;
    app_new(25);
    screen = 25;
    block_game.paused = 0;
    int before_y = block_game.y;
    ui_input(&tick);
    ui_input(&tick);
    assert(block_game.y > before_y);
    apps_pause();
    screen = 12;
    wifi_scanning = 1;
    ui_input(&tick);
    ui_input(&tick);
    assert(!wifi_scanning);
    timer_due = 0;
    screen = 29;
    history_count = 1;
    history[0] = 20;
    note_selected = -1;
    extra_commit(30, "Einkauf\nÄpfel, Öl und Bücher.");
    assert(note_count == 1);
    int id = note_selected;
    ui_draw();
    tap(28, 22);
    assert(screen == 29);
    extra_open(30);
    extra_open(43);
    tap(300, 550);
    assert(screen == 30 && history[history_count - 1] == 29);
    note_text[0] = 0;
    note_load(id);
    assert(strstr(note_text, "Äpfel"));
    screen = 30;
    ui_draw();

    open_keyboard(30, "Notiz", note_text);
    tap(540, 420);
    assert(edit[strlen(edit) - 1] == '\n');
    keyboard = 0;
    screen = 31;
    drawing_load();
    event(INPUT_DOWN, 100, 250);
    event(INPUT_MOVE, 200, 300);
    event(INPUT_TAP, 200, 300);
    assert(drawing[(250 - 112) * DRAW_W + 100] < 32);
    memset(drawing, 255, sizeof(drawing));
    drawing_load();
    assert(drawing[(250 - 112) * DRAW_W + 100] < 32);
    ui_draw();
    assert(ui_pixels[250 * 600 + 100] < 32);
    drawing_slot = 1;
    drawing_load();
    assert(drawing[(250 - 112) * DRAW_W + 100] == 255);
    event(INPUT_DOWN, 28, 22);
    event(INPUT_MOVE, 100, 250);
    event(INPUT_TAP, 100, 250);
    assert(drawing[(250 - 112) * DRAW_W + 100] == 255);
    event(INPUT_DOWN, 100, 250);
    event(INPUT_MOVE, 200, 300);
    event(INPUT_TAP, 200, 750);
    assert(screen == 31 && down_y == -1);
    calendar_day = 31;
    extra_commit(40, "2028-02");
    assert(calendar_year == 2028 && calendar_month == 2 && calendar_day == 29);
    apps_note_commit("Schalttag");
    assert(calendar_note_days & (1u << 28));
    screen = 45;
    tap(300, 480);
    assert(screen == 21 && !*calendar_note);
    const char *states =
        "[{\"entity_id\":\"light.desk\",\"state\":\"on\",\"attributes\":{\"friendly_name\":"
        "\"Leselicht\"}},{\"entity_id\":\"switch.heater\",\"state\":\"off\",\"attributes\":{"
        "\"friendly_name\":\"Heizung\"}},{\"entity_id\":\"climate.room\",\"state\":\"heat\"}]";
    extra_write_text("ha-states.json", states);
    ha_parse();
    assert(ha_count == 2 && !strcmp(ha_entities[0].name, "Leselicht"));
    strcpy(ha_url, "http://homeassistant.local:8123");
    strcpy(ha_token, "test-only");
    screen = 33;
    ha_select = 1;
    tap(530, 222);
    assert(ha_favorite_count == 1 && !strcmp(ha_favorites[0], "light.desk"));
    tap(530, 222);
    assert(ha_favorite_count == 0);
    screen = 34;
    open_keyboard(32, "Zugriffstoken", "AAAAAAAAAAAA");
    ui_draw();
    unsigned char *secret = malloc(sizeof(ui_pixels));
    memcpy(secret, ui_pixels, sizeof(ui_pixels));
    strcpy(edit, "BBBBBBBBBBBB");
    ui_draw();
    assert(!memcmp(secret, ui_pixels, sizeof(ui_pixels)));
    memset(edit, 'C', sizeof(edit) - 1);
    edit[sizeof(edit) - 1] = 0;
    ui_draw();
    free(secret);
    keyboard = 0;
    edit[0] = 0;
    assert(ui_open(books[0]));
    annotation_count = 1;
    memset(annotations, 0, sizeof(annotations));
    strcpy(annotations[0].text, "Ein markiertes Zitat.");
    engine_position(page, &annotations[0].position);
    assert(annotations_write());
    quotes_load();
    GlobalQuote quote;
    assert(global_quote_count == 1 && quote_get(0, &quote) &&
           !strcmp(quote.text, "Ein markiertes Zitat."));
    screen = 38;
    ui_draw();
    tap(100, 180);
    assert(screen == 2 && current == books[0]);
    const char *xml =
        "<api><query><pages><page title=\"Testartikel\"><extract>Erster Absatz.\n\n== Ein "
        "Abschnitt ==\nZweiter Absatz.</extract></page></pages></query></api>";
    char p[2048];
    local_path(p, sizeof(p), "wiki-fixture.xml");
    assert(atomic_write(p, xml, strlen(xml)));
    assert(discover_parse_lookup(p));
    wiki_paginate();
    assert(wiki_pages == 1);
    char line[200];
    int heading_flag;
    wiki_line("== Ein Abschnitt ==\nText", line, sizeof(line), &heading_flag);
    assert(heading_flag && !strcmp(line, "Ein Abschnitt"));
    screen = 36;
    ui_draw();
    options[OPT_BATTERY] = 0;
    options[OPT_BATTERY_PERCENT] = 1;
    screen = 1;
    header();
    assert(status_battery_x >= 0);
    options[OPT_BATTERY] = 1;
    options[OPT_BATTERY_PERCENT] = 0;
    header();
    assert(status_battery_x >= 0);
    extra_track(120, 2, library[books[0]].id);
    BookInsight *insight = insight_get(library[books[0]].id, 0);
    assert(insight && insight->seconds >= 120 && insight->pages >= 2);
    extra_save();
    char insight_path[2048];
    local_path(insight_path, sizeof(insight_path), "reading-insights-v1.bin");
    assert(!access(insight_path, R_OK));
    library_defaults();
    assert(options[OPT_APPS] && !options[OPT_TAB_LABELS] && settings.sort == 2 &&
           settings.size == 24);
    note_load(id);
    assert(strstr(note_text, "Äpfel"));
    storage_scan();
    assert(storage_total > 0 && storage_books > 0);
    assert(library_count >= 3);
    Annotation quote_rows[128];
    memset(quote_rows, 0, sizeof(quote_rows));
    for (int i = 0; i < 128; i++)
        snprintf(quote_rows[i].text, sizeof(quote_rows[i].text), "Zitat %d", i);
    for (int b = 0; b < 3; b++) {
        char path[2048];
        snprintf(path, sizeof(path), "%s/books/%016llx.notes-v1", data_root,
                 (unsigned long long)library[b].id);
        assert(atomic_write(path, quote_rows, sizeof(quote_rows)));
    }
    quotes_load();
    assert(global_quote_count >= 384 && quote_get(383, &quote));
    char *long_article = malloc(80500);
    assert(long_article);
    int bytes = snprintf(long_article, 80500, "<api><query><pages><page title='Long'><extract>");
    for (int i = 0; i < 40000; i++) {
        long_article[bytes++] = (char)0xc3;
        long_article[bytes++] = (char)0xa4;
    }
    bytes +=
        snprintf(long_article + bytes, 80500 - bytes, "</extract></page></pages></query></api>");
    local_path(p, sizeof(p), "wiki-long.xml");
    assert(atomic_write(p, long_article, bytes));
    free(long_article);
    assert(discover_parse_lookup(p));
    assert(strlen(discover_lookup_text()) > 60000 && strlen(discover_lookup_text()) < 65536);
    const char *u = discover_lookup_text();
    while (*u)
        assert(utf8_next(&u) == 0xe4);
    wiki_paginate();
    assert(wiki_pages > 2);
    engine_shutdown();
    puts("PASS: editable calculator, interruption/resume and keyboard preservation, alarm/sleep, "
         "persistent notes/drawings, calendar jump/clear, HA selection and token masking (8191 "
         "chars), global quotes, rich wiki sections, independent battery settings, reading "
         "insights, storage and non-destructive defaults.");
}
