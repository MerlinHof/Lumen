/* Public documentation previews use generated demo books only. */
#define _GNU_SOURCE
#include "../../lumen/system/ui/ui.c"
#include <assert.h>
static void picture(const char *name) {
    notification_dismiss();
    ui_draw();
    char path[256];
    snprintf(path, sizeof(path), "build/gallery/%s.pgm", name);
    FILE *f = fopen(path, "wb");
    assert(f);
    fprintf(f, "P5\n600 800\n255\n");
    fwrite(ui_pixels, 1, sizeof(ui_pixels), f);
    fclose(f);
}
int main(void) {
    library_root = "build/gallery/books";
    data_root = "build/gallery/data";
    assert(engine_init());
    library_scan();
    ui_init();
    options[OPT_LANGUAGE] = 1;
    options[OPT_KEYBOARD_LANGUAGE] = 1;
    if (library_count) {
        library[0].lastread = time(NULL);
        library[0].page = 14;
        library[0].total = 32;
        library_save_book(&library[0]);
    }
    update_view();
    route_to(1);
    picture("library");
    route_to(20);
    picture("apps");
    route_to(27);
    snprintf(calc_expr, sizeof(calc_expr), "sin(pi/3)^2+sqrt(5^2+3^2)");
    calc_cursor = strlen(calc_expr);
    calc_degrees = 0;
    calc_run();
    picture("calculator");
    route_to(13);
    picture("settings");
    warehouse_new17(1);
    route_to(118);
    picture("sokoban");
    if (library_count) {
        ui_open(0);
        page = engine_count() > 1 ? 1 : 0;
        reader_ui.reader_chrome = 0;
        picture("reader");
    }
    route_to(29);
    note_selected = -1;
    extra_commit(30, "A little room to think\n\nWhat made today feel different?\n\nA walk without "
                     "headphones.\nA chapter before breakfast.\nAn idea worth keeping.\n\nSmall "
                     "things, written down.");
    route_to(30);
    picture("notes");
    open_keyboard(30, "Note", note_text);
    editor_ui.note_cursor = strlen(editor_ui.edit);
    picture("keyboard");
    editor_ui.keyboard = 0;
    editor_ui.keyboard = 0;
    note_selected = -1;
    extra_commit(30, "Small things worth keeping\n\nA page. A walk. An unhurried morning.");
    note_tree.current = 0;
    route_to(29);
    picture("notes-list");
    task_count = 3;
    task_tab = task_board = task_page = 0;
    todo_ui.task_context16[0] = 0;
    snprintf(tasks[0].title, 200, "Read one chapter\nLeave a sentence in the journal");
    tasks[0].state = 0;
    snprintf(tasks[1].title, 200, "Try a recipe for the weekend");
    tasks[1].state = 0;
    snprintf(tasks[2].title, 200, "Take a slow walk");
    tasks[2].state = 2;
    route_to(74);
    picture("todo");
    chat_clear();
    chat_append(0, "How can I make more room for reading?");
    chat_append(1,
                "Start small. Choose a quiet moment that already belongs to you: the first cup of "
                "tea, a train journey, or the last few minutes before sleep.\n\nOne chapter is "
                "enough. Leave your book somewhere visible and let the next page be an invitation, "
                "not another task.\n\nA little consistency matters more than a perfect routine.");
    route_to(122);
    chat_page = 0;
    picture("chat");
    route_to(12);
    wifi_ui.wifi_enabled = 1;
    wifi_count = 3;
    wifi_ui.wifi_page = 0;
    snprintf(wifi_ui.wifi_ssid, sizeof(wifi_ui.wifi_ssid), "Home network");
    snprintf(wifi_networks[0].ssid, sizeof(wifi_networks[0].ssid), "Home network");
    wifi_networks[0].saved = 1;
    snprintf(wifi_networks[1].ssid, sizeof(wifi_networks[1].ssid), "A saved network");
    wifi_networks[1].saved = 1;
    snprintf(wifi_networks[2].ssid, sizeof(wifi_networks[2].ssid), "Guest network");
    snprintf(wifi_networks[2].flags, sizeof(wifi_networks[2].flags), "WPA2");
    picture("wifi");
    ui_close();
    engine_shutdown();
    return 0;
}
