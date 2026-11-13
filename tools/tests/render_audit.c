/* Reproducible native quality audit: demo data only; never uses Kindle data. */
#define _GNU_SOURCE
#include "../../lumen/system/ui/ui.c"
#include <assert.h>
#include <sys/time.h>
static const char *audit_dir;
static double audit_clock(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}
static void capture(const char *name) {
    notification_dismiss();
    editor_ui.keyboard = dropdown = 0;
    double t = audit_clock();
    for (int i = 0; i < 8; i++)
        ui_draw();
    double ms = (audit_clock() - t) * 1000 / 8;
    char p[512];
    snprintf(p, sizeof(p), "%s/%s.pgm", audit_dir, name);
    FILE *f = fopen(p, "wb");
    assert(f);
    fprintf(f, "P5\n600 800\n255\n");
    fwrite(ui_pixels, 1, sizeof(ui_pixels), f);
    fclose(f);
    printf("%s %.3f ms\n", name, ms);
    fflush(stdout);
}
int main(int argc, char **argv) {
    audit_dir = argc > 1 ? argv[1] : "build/audit/current";
    mkdir(audit_dir, 0700);
    static char demo_data[1024];
    snprintf(demo_data, sizeof(demo_data), "%s/data", audit_dir);
    library_root = "build/gallery/books";
    data_root = demo_data;
    mkdir(data_root, 0700);
    assert(engine_init());
    library_scan();
    ui_init();
    options[OPT_LANGUAGE] = 1;
    options[OPT_KEYBOARD_LANGUAGE] = 1;
    if (library_count) {
        library[0].lastread = time(NULL);
        library[0].page = 14;
        library[0].total = 32;
    }
    note_selected = -1;
    extra_commit(30, "A quiet morning\n\nOne idea worth keeping.");
    for (int id = 0; id < APP_COUNT; id++) {
        const AppDefinition *a = app_definition(id);
        if (a->entry < 0 || !app_route_valid(a->entry) || id == APP_DRAWING ||
            id == APP_NUMSCRATCH || id == APP_READER)
            continue;
        char name[80];
        snprintf(name, 80, "app-%02d", id);
        route_to(a->entry);
        capture(name);
    }
    route_to(12);
    wifi_ui.wifi_enabled = 1;
    wifi_count = 3;
    snprintf(wifi_ui.wifi_ssid, sizeof(wifi_ui.wifi_ssid), "Home network");
    snprintf(wifi_networks[0].ssid, 128, "Home network");
    wifi_networks[0].saved = 1;
    snprintf(wifi_networks[1].ssid, 128, "A saved network");
    wifi_networks[1].saved = 1;
    snprintf(wifi_networks[2].ssid, 128, "Guest network");
    snprintf(wifi_networks[2].flags, sizeof(wifi_networks[2].flags), "WPA2");
    capture("wifi");
    for (int lang = 0; lang < 3; lang++) {
        options[OPT_LANGUAGE] = lang;
        route_to(13);
        char name[80];
        snprintf(name, 80, "settings-%d", lang);
        capture(name);
        const char *sections[]={"wifi","appearance","date-time","manage","system"};
        for (int section=0;section<5;section++) {
            char event[100];
            snprintf(event,sizeof(event),"{\"event\":\"component\",\"id\":\"%s\",\"gesture\":\"clicked\"}",sections[section]);
            app_process_send(event);
            for(int i=0;i<20;i++){usleep(5000);package_ui_poll();}
            snprintf(name,80,"settings-%d-%d",lang,section);
            capture(name);
            app_process_send("{\"event\":\"back\"}");
            for(int i=0;i<20;i++){usleep(5000);package_ui_poll();}
        }
    }
    options[OPT_LANGUAGE] = 1;
    route_to(2);
    app_process_open(APP_READER);
    app_process_send("{\"event\":\"input\",\"kind\":1,\"x\":300,\"y\":55}");
    app_process_send("{\"event\":\"input\",\"kind\":1,\"x\":550,\"y\":95}");
    for(int i=0;i<30;i++){usleep(5000);package_ui_poll();}
    for (int p = 0; p < 2; p++) {
        char event[64];snprintf(event,sizeof(event),"{\"event\":\"page\",\"page\":%d}",p);
        app_process_send(event);
        for(int i=0;i<20;i++){usleep(5000);package_ui_poll();}
        char name[40];
        snprintf(name, 40, "reading-%d", p);
        capture(name);
    }
    route_to(27);
    app_process_open(APP_CALCULATOR);
    app_process_send("{\"event\":\"text\",\"action\":80,\"text\":\"e^(0.2*sin(pi))+1/3\"}");
    for(int i=0;i<30;i++){usleep(5000);package_ui_poll();}
    capture("calculator");
    for (int p = 0; p < 4; p++) {
        route_to(21);
        app_process_open(APP_CALENDAR);
        char selection[128];
        snprintf(selection,sizeof(selection),"{\"event\":\"component\",\"id\":\"view\",\"gesture\":\"selected\",\"text\":\"%d\"}",p);
        app_process_send(selection);
        for(int i=0;i<30;i++){usleep(5000);package_ui_poll();}
        char name[40];
        snprintf(name, 40, "calendar-%d", p);
        capture(name);
    }

    route_to(94);
    wiki_entry_count = 3;
    for (int i = 0; i < 3; i++) {
        snprintf(wiki_entries[i].title, 256, "%s",
                 i == 0   ? "A longer article title with room to breathe"
                 : i == 1 ? "Rosenheim"
                          : "Typography");
        wiki_entries[i].seen = time(NULL) - i * 86400;
    }
    capture("filled-wikipedia");
    route_to(95);
    press_list.count = 5;
    for (int i = 0; i < 5; i++) {
        snprintf(press_list.item[i].title, 256, "%s",
                 i % 2
                     ? "A discovery worth taking a moment to understand"
                     : "A longer newspaper headline that wraps into two clear, well-spaced lines");
        snprintf(press_list.item[i].publisher, 96, "Example Gazette");
        snprintf(press_list.item[i].category, 48, "Wissenschaft");
    }
    capture("filled-news");
    route_to(10);
    opds_count = 5;
    for (int i = 0; i < 5; i++) {
        snprintf(opds_items[i].title, 256, "%s",
                 i % 2 ? "The Quiet Hours"
                       : "A longer book title that wraps gracefully without touching its author");
        snprintf(opds_items[i].author, 160, "Hof, Merlin");
    }
    capture("filled-store");
    route_to(114);
    recipe_recent_count16 = 5;
    for (int i = 0; i < 5; i++)
        snprintf(recipe_recent16[i].title, 160, "%s",
                 i % 2 ? "Kartoffelsuppe"
                       : "Ein ausführlicher Rezepttitel mit frischen Zutaten aus der Region");
    capture("filled-recipes");
    route_to(33);
    snprintf(ha_url, sizeof(ha_url), "http://example.invalid");
    snprintf(ha_token, sizeof(ha_token), "demo-only");
    ha_count = ha_favorite_count = 3;
    for (int i = 0; i < 3; i++) {
        snprintf(ha_entities[i].id, 160, "sensor.demo_%d", i);
        snprintf(ha_entities[i].name, 192, "%s",
                 i % 2 ? "Living room temperature" : "A useful sensor with a slightly longer name");
        snprintf(ha_entities[i].state, 64, "21.5");
        snprintf(ha_entities[i].unit, 32, "°C");
        ha_entities[i].sensor = 1;
        snprintf(ha_favorites[i], 160, "%s", ha_entities[i].id);
    }
    app_process_open(APP_HOME_ASSISTANT);
    for(int i=0;i<20;i++){usleep(5000);app_process_poll();}
    const Input choose_devices={.kind=INPUT_TAP,.x=549,.y=94};
    package_ui_input(&choose_devices);
    for(int i=0;i<20;i++){usleep(5000);package_ui_poll();}
    capture("filled-devices");
    app_process_send("{\"event\":\"back\"}");
    for(int i=0;i<20;i++){usleep(5000);package_ui_poll();}
    capture("filled-home-assistant");
    route_to(83);
    capture("launcher-management");
    for (int pg = 0; pg < 2; pg++) {
        route_to(40);
        extra_page = pg;
        char name[40];
        snprintf(name, 40, "system-%d", pg);
        capture(name);
    }
    route_to(100);
    capture("calculator-settings");
    route_to(28);
    app_process_open(APP_TIMER);
    app_process_send("{\"event\":\"component\",\"id\":\"mode\",\"text\":\"1\"}");
    for(int i=0;i<30;i++){usleep(5000);package_ui_poll();}
    capture("alarm");
    route_to(29);
    app_process_open(APP_NOTES);
    app_process_send("{\"event\":\"component\",\"id\":\"entry-0\",\"gesture\":\"clicked\"}");
    for(int i=0;i<20;i++){usleep(5000);package_ui_poll();}
    capture("empty-folder");
    if (library_count) {
        ui_open(0);
        reader_ui.reader_chrome = 1;
        capture("reader-controls");
    }
    ui_close();
    engine_shutdown();
    return 0;
}
