#define _GNU_SOURCE
#include "system/device.h"
#include "system/version.h"
#include "system/engine.h"
#include "system/library.h"
#include "system/services.h"
#include "system/wireless_update.h"
#include "system/system_update.h"
#include "system/ui/api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
static double stamp(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}
int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--version")) {
        puts("LUMEN " LUMEN_VERSION " / MuPDF 1.28.5 / native ARMv7");
        return 0;
    }
    if (argc == 2 && !strcmp(argv[1], "--apply-wireless"))
        return update_apply() ? 0 : 1;
    if(argc==2 && !strcmp(argv[1],"--system-maintenance"))return system_update_maintenance()?0:1;
    if(argc==2 && !strcmp(argv[1],"--system-rollback"))return system_update_rollback()?0:1;
    const char *preview = NULL, *inspect = NULL, *render = NULL;
    int screen = 0, pageno = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--library") && i + 1 < argc)
            library_root = argv[++i];
        else if (!strcmp(argv[i], "--data") && i + 1 < argc)
            data_root = argv[++i];
        else if (!strcmp(argv[i], "--preview") && i + 1 < argc)
            preview = argv[++i];
        else if (!strcmp(argv[i], "--screen") && i + 1 < argc)
            screen = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--inspect") && i + 1 < argc)
            inspect = argv[++i];
        else if (!strcmp(argv[i], "--render") && i + 1 < argc)
            render = argv[++i];
        else if (!strcmp(argv[i], "--page") && i + 1 < argc)
            pageno = atoi(argv[++i]);
        else {
            fprintf(stderr, "Unknown argument: %s\n", argv[i]);
            return 1;
        }
    }
    if (!engine_init())
        return 2;
    if (inspect) {
        char title[384] = "", author[192] = "";
        engine_metadata(inspect, title, sizeof(title), author, sizeof(author));
        if (!engine_open(inspect, 22, 145, 12)) {
            fprintf(stderr, "%s\n", engine_error());
            engine_shutdown();
            return 3;
        }
        Chapter chapters[256];
        int n = engine_outline(chapters, 256);
        printf("title=%s\nauthor=%s\npages=%d\nreflowable=%d\nchapters=%d\n", title, author,
               engine_count(), engine_reflowable(), n);
        for (int i = 0; i < n; i++)
            printf("%d:%s\n", chapters[i].page + 1, chapters[i].title);
        if (render) {
            unsigned char pixels[544 * 580];
            if (pageno < 0 || pageno >= engine_count() ||
                !engine_render(pageno, pixels, 544, 544, 580, 1, 0, 0)) {
                engine_shutdown();
                return 4;
            }
            FILE *f = fopen(render, "wb");
            if (!f) {
                engine_shutdown();
                return 5;
            }
            fprintf(f, "P5\n544 580\n255\n");
            fwrite(pixels, 1, sizeof(pixels), f);
            fclose(f);
        }
        engine_shutdown();
        return 0;
    }
    library_settings_load();
    double start = stamp();
    if (!preview) {
        if (!device_init()) {
            service_cancel();
            device_close();
            engine_shutdown();
            return 6;
        }
        ui_loading("Bibliothek wird vorbereitet ...");
        device_present(ui_pixels, 1, settings.refresh);
    }
    library_scan();
    ui_init();
    if (preview) {
        ui_set_screen(screen);
        if (screen == 2 && library_count)
            ui_open(0);
        ui_draw();
        FILE *f = fopen(preview, "wb");
        if (!f) {
            engine_shutdown();
            return 5;
        }
        fprintf(f, "P5\n600 800\n255\n");
        fwrite(ui_pixels, 1, sizeof(ui_pixels), f);
        fclose(f);
        ui_close();
        engine_shutdown();
        return 0;
    }
    ui_draw();
    device_present(ui_pixels, 1, settings.refresh);
    fprintf(stderr, "startup_ms=%.1f books=%d\n", (stamp() - start) * 1000, library_count);
    int profile = getenv("LUMEN_PROFILE") && !strcmp(getenv("LUMEN_PROFILE"), "1");
    Input input;
    device_poll_interval(ui_poll_interval());
    int redraw_pending = 0;
    while (device_wait(&input)) {
        double began = stamp();
        int action = ui_input(&input);
        if (!action)
            break;
        if (action == 1)
            redraw_pending = 1;
        int poll = ui_poll_interval();
        device_poll_interval(redraw_pending && (poll < 0 || poll > 24) ? 24 : poll);
        if ((action == 3 || redraw_pending) && device_input_pending())
            continue;
        if ((action == 3 || redraw_pending) && device_refresh_busy())
            continue;
        if (action == 3 && !redraw_pending) {
            ui_feedback_draw();
            device_present_async(ui_pixels);
            continue;
        }
        if (!redraw_pending)
            continue;
        redraw_pending = 0;
        ui_draw();
        int full_refresh = ui_full_refresh();
        if (!full_refresh)
            device_present_async(ui_pixels);
        else
            device_present(ui_pixels, full_refresh, settings.refresh);
        if (profile)
            fprintf(stderr, "action=%d render_ms=%.1f\n", input.kind, (stamp() - began) * 1000);
    }
    int power_request = ui_power_request();
    if (power_request) {
        ui_power_screen(power_request == 2);
        device_present(ui_pixels, 1, settings.refresh);
    }
    ui_save();
    service_cancel();
    device_close();
    ui_close();
    engine_shutdown();
    fprintf(stderr, "LUMEN " LUMEN_VERSION " clean exit\n");
    if (power_request)
        device_system_power(power_request == 2);
    return 0;
}
