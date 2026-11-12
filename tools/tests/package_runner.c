#include "../../lumen/system/packages.h"
#include "../../lumen/system/system_update.h"
#include "../../lumen/system/app_packages.h"
#include "../../lumen/system/library.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
static void wait_for_frame(uint64_t previous) {
    for (int i=0;i<100 && app_process_frame_serial()==previous;i++) {
        usleep(10000);
        app_process_poll();
    }
    assert(app_process_frame_serial()!=previous);
}
static int test_wire(void) {
    app_packages_scan();
    int id=app_package_find("test.wire");
    assert(id>=0 && app_process_open(id));
    wait_for_frame(0);
    assert(app_process_frame_back());
    assert(strstr(app_process_frame(),"nested"));
    assert(!*app_process_request());
    uint64_t serial=app_process_frame_serial();
    assert(app_process_send("{}"));
    wait_for_frame(serial);
    assert(!app_process_frame_back());
    assert(strstr(app_process_frame(),"still valid"));
    assert(!*app_process_request());
    assert(app_process_send("{}"));
    for (int i=0;i<100;i++) {
        usleep(10000);
        app_process_poll();
        if (strstr(app_process_request(),"notify")) {
            app_process_close();
            puts("PASS app IPC distinguishes top-level requests, frame text, malformed JSON and Boolean Back state.");
            return 0;
        }
    }
    assert(!"App request did not arrive");
    return 2;
}
int main(int argc,char**argv){assert(argc>=3);data_root=argv[1];const char*op=argv[2];
 if(!strcmp(op,"wire"))return test_wire();
 if(!strcmp(op,"stage"))return system_update_stage(argv[3],argc>4 ? argv[4] : "9.1")?0:2;
 if(!strcmp(op,"apply"))return system_update_maintenance()?0:2;
 if(!strcmp(op,"rollback"))return system_update_rollback()?0:2;
 if(!strcmp(op,"keep"))return system_update_keep()?0:2;
 if(!strcmp(op,"install"))return app_package_install(argv[3])?0:2;
 if(!strcmp(op,"launch")){app_packages_scan();assert(app_package_installed(APP_SIGNAL));assert(app_process_open(APP_SIGNAL));for(int i=0;i<100&&!*app_process_frame();i++){usleep(10000);app_process_poll();}assert(strstr(app_process_frame(),"Schiebepuzzle"));assert(app_process_send("{\"event\":\"input\",\"kind\":1,\"x\":550,\"y\":90}"));for(int i=0;i<30;i++){usleep(10000);app_process_poll();}assert(!*app_process_error());app_process_close();puts("PASS independent app process renders and handles input");return 0;}
 return 3;}
