#define _GNU_SOURCE
#include "device.h"
#include "version.h"
#include "engine.h"
#include "library.h"
#include "services.h"
#include "wireless_update.h"
#include "system_update.h"
#include "app_packages.h"
#include "ui/api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "html_fragment.h"
#include "markup_tool.h"
#include <math.h>
#include "render_document.h"
static double stamp(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}
int main(int argc, char **argv) {
    if(argc==7 && !strcmp(argv[1],"--render-document"))
        return render_document(argv[2],atoi(argv[3]),atoi(argv[4]),atoi(argv[5]),argv[6]) ? 0 : 1;
    if(argc>=3 && argc<=43 && !strcmp(argv[1],"--markup-tree")) {
        if(strcmp(argv[2],"html") && strcmp(argv[2],"xml"))return 1;
        return markup_tool(!strcmp(argv[2],"html"),argc-3,argv+3) ? 0 : 1;
    }
    if(argc==6 && !strcmp(argv[1],"--document-image")) {
        int w=atoi(argv[4]),h=atoi(argv[5]);
        if(w<1 || w>800 || h<1 || h>800 || !engine_init())return 1;
        int ok=engine_image_fit(argv[2],argv[3],w,h);
        engine_shutdown();
        return ok ? 0 : 1;
    }
    if(argc==3 && !strcmp(argv[1],"--sanitize-html"))return sanitize_html(argv[2]) ? 0 : 1;
    char *self=realpath(argv[0],NULL);
    if(self){setenv("LUMEN_DOCUMENT_TOOL",self,1);free(self);}

    if (argc == 2 && !strcmp(argv[1], "--version")) {
        puts("LUMEN " LUMEN_VERSION " / MuPDF 1.28.5 / native ARMv7");
        return 0;
    }
    if (argc == 2 && !strcmp(argv[1], "--apply-wireless"))
        return update_apply() ? 0 : 1;
    if(argc==2 && !strcmp(argv[1],"--system-maintenance"))return system_update_maintenance()?0:1;
    if(argc==2 && !strcmp(argv[1],"--system-rollback"))return system_update_rollback()?0:1;
    const char *preview = NULL, *inspect = NULL, *render = NULL;
    const char *app=NULL;
    int pageno=0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--library") && i + 1 < argc)
            library_root = argv[++i];
        else if (!strcmp(argv[i], "--data") && i + 1 < argc)
            data_root = argv[++i];
        else if (!strcmp(argv[i], "--preview") && i + 1 < argc)
            preview = argv[++i];
        else if (!strcmp(argv[i], "--app") && i+1<argc)
            app=argv[++i];
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
        if(system_update_trial())ui_update_progress(5,100,LUMEN_VERSION,NULL);
        else ui_loading("Bibliothek wird vorbereitet ...");
        device_present(ui_pixels, 1, settings.refresh);
    }
    library_scan();
    ui_init();
    if((!preview || !app) && !ui_prepare_home() && !preview && system_update_trial()) {
        ui_close();
        device_close();
        engine_shutdown();
        return 7;
    }
    if (preview) {
        if(app && !ui_select_app(app)) { fprintf(stderr,"Unknown foreground app: %s\n",app); return 1; }
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
    const char *recovered=getenv("LUMEN_UPDATE_RECOVERED");
    if(recovered && !strcmp(recovered,"1"))
        ui_message("Update fehlgeschlagen. Vorige Version wiederhergestellt.");
    ui_draw();
    device_present(ui_pixels, 1, settings.refresh);
#ifndef LUMEN_HOST
    if(system_update_trial()) {
        if(system_update_keep()) {
            unlink("/mnt/us/lumen/.update/system-starting");
            app_packages_seed();
            ui_prepare_home();
            device_finish_update();
        }
    }
#endif
    fprintf(stderr, "startup_ms=%.1f books=%d\n", (stamp() - start) * 1000, library_count);
    int profile = getenv("LUMEN_PROFILE") && !strcmp(getenv("LUMEN_PROFILE"), "1");
    Input input;
    device_poll_interval(ui_poll_interval());
    int redraw_pending = 0, feedback_pending = 0, feedback_shown = 0;
    while (device_wait(&input)) {
        double began = stamp();
        int action = ui_input(&input);
        if (!action)
            break;
        if (action == 1) redraw_pending = 1;
        if (action == 3) feedback_pending = 1;
        if (!ui_feedback_active()) {
            feedback_pending = 0;
            if (feedback_shown) redraw_pending = 1;
        }
        int poll = ui_poll_interval();
        if ((redraw_pending || feedback_pending) && (poll < 0 || poll > 16)) poll=16;
        device_poll_interval(poll);
        if ((feedback_pending || redraw_pending) && device_refresh_busy()) continue;
        if (feedback_pending && !redraw_pending) {
            feedback_pending = 0;
            ui_feedback_draw();
            device_present_async(ui_pixels);
            feedback_shown = 1;
            continue;
        }
        if (!redraw_pending || device_input_pending() || ui_defer_app_frame())
            continue;
        redraw_pending = feedback_pending = 0;
        ui_draw();
        feedback_shown = ui_feedback_active();
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
