#define _GNU_SOURCE
#include "../src/system/ui/ui.c"
#include <assert.h>
static void tap17(int x, int y) {
    Input in = {.kind = INPUT_TAP, .x = x, .y = y};
    ui_input(&in);
    ui_draw();
}
static void shot17(const char *n) {
    notification_dismiss();
    ui_draw();
    char p[256];
    snprintf(p, 256, "build/tests/%s.pgm", n);
    FILE *f = fopen(p, "wb");
    fprintf(f, "P5\n600 800\n255\n");
    fwrite(ui_pixels, 1, 480000, f);
    fclose(f);
}
#include "test_input_ui.h"
#include "test_improvements.h"
#include "test_quality.h"
int main(int argc, char **argv) {
    (void)argv;
    library_root = "build/tests/fixtures";
    data_root = "build/tests/test-data";
    mkdir(data_root, 0700);
    assert(engine_init());
    ui_init();
    assert(!app_route_valid(124));
    if (argc > 1) {
        selected_recipe17.meal = 0;
        strcpy(selected_recipe17.lang, "de");
        strcpy(selected_recipe17.id, "wiki-de-115892");
        strcpy(selected_recipe17.title, "Rote-Bete-Gnocchi mit Spinat");
        assert(recipes_fetch17(""));
        recipe_archive16();
        assert(recipe_recent_count16 > 0);
        char path[4096];
        snprintf(path, sizeof(path), "%s/recipe-archive16/wiki-de-115892.html", data_root);
        assert(file_open16(path, 114));
        assert(engine_count() > 0);
        ui_draw();
        shot17("recipe-live");
        ui_close();
        engine_shutdown();
        puts("PASS native German recipe fetch, rich HTML, image preparation, offline archive and "
             "Reader");
        return 0;
    }
    route_to(29);
    ui_draw();
    u16_notes_defaults();
    note_tree.current = note_tree.folder[0].id;
    ui_draw();
    assert(u17_notes_depth());
    tap17(20, 20);
    assert(!note_tree.current);
    note_selected = -1;
    extra_commit(30, "Title\nSome text to edit.");
    int id = note_selected;
    route_to(29);
    ui_draw();
    shot17("notes");
    u17_notes_menu(0, id, 150);
    ui_draw();
    tap17(drop_x + 100, drop_y + 25);
    assert(dropdown && drop_target == 936);
    ui_draw();
    shot17("folder-popup");
    tap17(drop_x + 100, drop_y + 5 + 48 + 20);
    assert(v1_folder_for(id) == note_tree.folder[0].id);
    task_count = 1;
    strcpy(tasks[0].title, "Task");
    route_to(74);
    ui_draw();
    tap17(160, 230);
    assert(keyboard && keyboard_target == 180 && tasks[0].state == 0);
    strcpy(edit, "Edited task");
    commit_keyboard();
    ui_draw();
    tap17(42, 235);
    assert(tasks[0].state == 2);
    shot17("todo");
    for (int i = 0; i < 12; i++) {
        strcpy(tasks[i].title, "Task");
        snprintf(task_categories16[i], 64, "Context %d", i);
    }
    task_count = 12;
    contexts16();
    route_to(74);
    ui_draw();
    tap17(250, 90);
    assert(dropdown && drop_count == 13 && drop_visible == 5);
    ui_draw();
    shot17("contexts");
    tap17(drop_x + drop_w - 20, drop_y + 5 + 5 * 48 + 15);
    assert(drop_first == 5);
    tap17(drop_x + 80, drop_y + 25);
    assert(!strcmp(task_context16, context_names16[5]));
    task_context16[0] = 0;
    route_to(100);
    ui_draw();
    shot17("calc-settings");
    tap17(400, 175);
    assert(dropdown && drop_target == 931);
    tap17(drop_x + 80, drop_y + 5 + 48 + 15);
    assert(!calc_degrees);
    route_to(40);
    extra_page = 1;
    ui_draw();
    tap17(450, 325);
    assert(dropdown && screen == 40 && drop_target == 917);
    dropdown = 0;
    note_load(id);
    extra_page = 0;
    route_to(30);
    ui_draw();
    shot17("editor");
    Input hold = {.kind = INPUT_HOLD, .x = 300, .y = 600};
    route_to(27);
    ui_draw();
    ui_input(&hold);
    assert(!clip_active);
    open_keyboard(180, "Task", "some text");
    ui_draw();
    hold.x = 100;
    hold.y = 540;
    ui_input(&hold);
    assert(!clip_active);
    keyboard = 0;
    for (int level = 1; level <= 20; level++) {
        warehouse_new17(level);
        int goals = 0, boxes = 0;
        for (int i = 0; i < warehouse17.w * warehouse17.h; i++) {
            goals += warehouse17.board[i] == 2 || warehouse17.board[i] == 5;
            boxes += box17(warehouse17.board[i]);
        }
        assert(goals == boxes && boxes > 0);
        assert(floor17(warehouse17.board[warehouse17.y * warehouse17.w + warehouse17.x]));
    }
    route_to(118);
    ui_draw();
    shot17("sokoban");
    maze_new17();
    int seen[1155] = {0}, queue[1155], head = 0, tail = 0;
    seen[34] = 1;
    queue[tail++] = 34;
    while (head < tail) {
        int at = queue[head++], dirs[] = {1, -1, 33, -33};
        for (int d = 0; d < 4; d++) {
            int next = at + dirs[d];
            if (next >= 0 && next < 1155 && !maze17[next] && !seen[next]) {
                seen[next] = 1;
                queue[tail++] = next;
            }
        }
    }
    assert(seen[33 * 33 + 31]);
    route_to(119);
    ui_draw();
    shot17("maze");
    route_to(120);
    ui_draw();
    shot17("crossword");
    crossword_pick16 = 0;
    cross_commit17("X");
    assert(!*crossword_answer16[0]);
    cross_commit17(cross_words16[0]);
    assert(!strcmp(crossword_answer16[0], cross_words16[0]));
    route_to(110);
    ui_draw();
    shot17("journal-list");
    int today = app_date_key(time(NULL));
    journal_open17(today);
    ui_draw();
    u17_more_commit(171, "My day\nSecond line");
    journal_score16 = 4;
    journal_store17();
    extra_return(110);
    journal_open17(today);
    assert(!strcmp(journal_answers16[0], "My day\nSecond line") && journal_score16 == 4);
    ui_draw();
    shot17("journal");
    extra_write_text("llm-models17.json", "{\"data\":[{\"id\":\"model-b\"},{\"id\":\"model-a\"}]}");
    models_parse17();
    assert(model_count17 == 2 && !strcmp(model_ids17[0], "model-a"));
    models_loaded17 = 1;
    extra_write_text("llm-key.txt", "dummy-visible-key");
    route_to(123);
    ui_draw();
    shot17("llm-settings");
    route_to(122);
    ui_draw();
    shot17("llm");
    reader_markdown17("Markdown test", "# Heading\n\nA **bold** paragraph.\n\n- One\n- Two\n", 122);
    assert(screen == 2 && reader_document_active);
    reader_chrome = 1;
    ui_draw();
    shot17("reader");
    int page_before = page;
    tap17(300, 300);
    assert(!reader_chrome && page == page_before);
    tap17(300, 55);
    assert(reader_chrome);
    tap17(100, 128);
    assert(screen == 3);
    route_to(2);
    reader_chrome = 1;
    ui_draw();
    tap17(510, 128);
    assert(screen == 104);
    calendar_view16 = 1;
    route_to(21);
    ui_draw();
    shot17("calendar-week");
    tap17(200, 350);
    assert(calendar_view16 == 0);
    ui_draw();
    shot17("calendar-day");
    route_to(79);
    extra_prepare(79);
    ui_draw();
    tap17(400, 90);
    assert(dropdown && drop_target == 933);
    tap17(drop_x + 90, drop_y + 5 + 48 + 20);
    assert(screen == 125);
    ui_draw();
    shot17("solar");
    for (int lang = 0; lang < 3; lang++) {
        options[OPT_LANGUAGE] = lang;
        route_to(123);
        ui_draw();
        shot17(lang == 0 ? "de" : lang == 1 ? "en" : "fr");
        assert(strcmp(tr_language("Dieser Ordner ist leer.", lang),
                      lang ? "Dieser Ordner ist leer." : "English") != 0);
    }
    options[OPT_LANGUAGE] = 0;
    input_tests18();
    improvements_tests();
    quality_tests();
    ui_close();
    engine_shutdown();
    puts("PASS compact popups/nested folder menu, notes back, task editing/checkboxes, calculator "
         "dropdown, inline sleep, clipboard hit limits, warehouse/maze, crossword, journal "
         "editing, model list, Markdown Reader/chrome, local calendar and planet selector.");
}
