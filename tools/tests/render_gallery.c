/* Public documentation previews use generated demo books only. */
#define _GNU_SOURCE
#include "../../lumen/system/ui/ui.c"
#include <assert.h>
static void picture(const char *name) {
    notification_dismiss();
    if(app_package_installed(app_for_route(ui_route()))) {
        app_process_open(app_for_route(ui_route()));
        for(int i=0;i<20;i++){usleep(5000);package_ui_poll();}
    }
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
    route_to(131);
    assert(ui_prepare_home());
    picture("home");
    app_process_send("{\"event\":\"page\",\"page\":1}");
    picture("apps");
    route_to(27);
    app_process_open(APP_CALCULATOR);
    app_process_send("{\"event\":\"component\",\"id\":\"settings\",\"gesture\":\"clicked\"}");
    app_process_send("{\"event\":\"select\",\"action\":1,\"value\":1}");
    app_process_send("{\"event\":\"back\"}");
    app_process_send("{\"event\":\"text\",\"action\":80,\"text\":\"sin(pi/3)^2+sqrt(5^2+3^2)\"}");
    app_process_send("{\"event\":\"component\",\"id\":\"equals\",\"gesture\":\"clicked\"}");
    for(int i=0;i<30;i++){usleep(5000);package_ui_poll();}
    picture("calculator");
    route_to(13);
    picture("settings");
    route_to(28);
    app_process_open(APP_TIMER);
    app_process_send("{\"event\":\"component\",\"id\":\"mode\",\"text\":\"0\"}");
    for(int i=0;i<30;i++){usleep(5000);package_ui_poll();}
    app_process_open(APP_TIMER);
    app_process_send("{\"event\":\"select\",\"action\":2,\"value\":0}");
    for(int i=0;i<30;i++){usleep(5000);package_ui_poll();}
    picture("clock");
    app_process_open(APP_TIMER);
    app_process_send("{\"event\":\"component\",\"id\":\"mode\",\"text\":\"2\"}");
    for(int i=0;i<30;i++){usleep(5000);package_ui_poll();}
    picture("alarm");
    app_process_open(APP_TIMER);
    app_process_send("{\"event\":\"component\",\"id\":\"mode\",\"text\":\"0\"}");
    for(int i=0;i<30;i++){usleep(5000);package_ui_poll();}
    for(int face=0;face<14;face++) {
        char name[40];snprintf(name,sizeof(name),"face-%02d",face);
        char selection[80];snprintf(selection,sizeof(selection),"{\"event\":\"select\",\"action\":2,\"value\":%d}",face);
        app_process_send(selection);
        for(int i=0;i<20;i++){usleep(5000);package_ui_poll();}
        notification_dismiss();ui_draw();
        char path[256];snprintf(path,sizeof(path),"build/gallery/%s.pgm",name);
        FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P5\n600 800\n255\n");fwrite(ui_pixels,1,sizeof(ui_pixels),f);fclose(f);
    }
    app_process_open(APP_TIMER);
    app_process_send("{\"event\":\"select\",\"action\":2,\"value\":0}");
    for(int i=0;i<30;i++){usleep(5000);package_ui_poll();}
    route_to(13);
    app_process_open(APP_SETTINGS);
    app_process_send("{\"event\":\"component\",\"id\":\"system\",\"gesture\":\"clicked\"}");
    picture("system-update");
    route_to(129);
    picture("sliding-puzzle");
    route_to(130);
    picture("frog-solitaire");
    route_to(118);
    picture("sokoban");
    if (library_count) {
        ui_open(0);
        page = engine_count() > 1 ? 1 : 0;
        reader_ui.reader_chrome = 0;
        picture("reader");
    }
    const char *demo_note="A little room to think\n\nWhat made today feel different?\n\n"
        "A walk without headphones.\nA chapter before breakfast.\nAn idea worth keeping.";
    lumen_storage_write_at(data_root,"notes/123.txt",demo_note,strlen(demo_note));
    route_to(29);
    app_process_open(APP_NOTES);
    app_process_send("{\"event\":\"intent\",\"operation\":\"note-open\",\"note_id\":123}");
    picture("notes");
    app_process_send("{\"event\":\"component\",\"id\":\"line-0\",\"gesture\":\"caret\",\"text\":\"21\"}");
    picture("keyboard");
    editor_ui.keyboard=0;
    app_process_send("{\"event\":\"back\"}");
    picture("notes-list");
    route_to(74);
    picture("todo");
    route_to(122);
    picture("chat");
    route_to(13);
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
    app_process_open(APP_SETTINGS);
    app_process_send("{\"event\":\"component\",\"id\":\"wifi\",\"gesture\":\"clicked\"}");
    picture("wifi");
    ui_close();
    engine_shutdown();
    return 0;
}
