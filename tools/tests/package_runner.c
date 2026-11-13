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
        if (strstr(app_process_request(),"hello")) {
            assert(strstr(app_process_request(),"second"));
            assert(!*app_process_request());
            app_process_close();
            puts("PASS app IPC distinguishes top-level requests, frame text, malformed JSON and Boolean Back state.");
            return 0;
        }
    }
    assert(!"App request did not arrive");
    return 2;
}
static int test_service(void) {
    app_packages_scan();
    int caller=app_package_find("test.caller");
    assert(caller>=0 && app_process_open(caller));
    wait_for_frame(0);
    assert(!app_service_start("lumen.study","undeclared","",4));
    assert(!app_service_start("test.missing","material_count","",4));
    assert(app_service_start("lumen.study","material_count","",6));
    assert(!app_service_start("lumen.study","material_count","",7));
    uint64_t serial=app_process_frame_serial();
    for(int i=0;i<100 && serial==app_process_frame_serial();i++) {
        usleep(10000);
        app_service_poll();
        app_process_poll();
    }
    assert(strstr(app_process_frame(),"success"));
    assert(strstr(app_process_frame(),"\"data\""));
    assert(app_process_owner()==caller && app_service_fd()<0);
    assert(app_service_start("lumen.study","material_count","invalid",8));
    serial=app_process_frame_serial();
    for(int i=0;i<100 && serial==app_process_frame_serial();i++) {
        usleep(10000);
        app_service_poll();
        app_process_poll();
    }
    assert(strstr(app_process_frame(),"error"));
    assert(app_service_start("test.worker","exit","",9));
    serial=app_process_frame_serial();
    for(int i=0;i<100 && serial==app_process_frame_serial();i++) {
        usleep(10000);
        app_service_poll();
        app_process_poll();
    }
    assert(strstr(app_process_frame(),"crashed"));
    assert(app_service_start("test.worker","reply_and_exit","",10));
    serial=app_process_frame_serial();
    for(int i=0;i<100 && serial==app_process_frame_serial();i++) {
        usleep(10000);
        app_service_poll();
        app_process_poll();
    }
    assert(strstr(app_process_frame(),"finished"));
    char recall[8193]="Physics\n";
    size_t used=strlen(recall);
    for(int i=0;i<1600;i++) {
        memcpy(recall+used,"A\n\t",3);
        used+=3;
    }
    recall[used]=0;
    assert(app_service_start("lumen.notes","create_learning",recall,11));
    serial=app_process_frame_serial();
    for(int i=0;i<100 && serial==app_process_frame_serial();i++) {
        usleep(10000);
        app_service_poll();
        app_process_poll();
    }
    assert(strstr(app_process_frame(),"success") && strstr(app_process_frame(),"\"action\":11"));
    memset(recall,'A',sizeof(recall)-1);
    recall[sizeof(recall)-1]=0;
    assert(!app_service_start("lumen.notes","create_learning",recall,12));
    assert(app_service_start("lumen.study","material_count","",9));
    app_process_pause();
    assert(app_service_fd()<0 && app_process_owner()==caller);
    app_process_close();
    puts("PASS manifest-declared app service, result, invalid input, process exit, cancellation and worker shutdown.");
    return 0;
}
int main(int argc,char**argv){assert(argc>=3);data_root=argv[1];const char*op=argv[2];
 if(!strcmp(op,"service"))return test_service();
 if(!strcmp(op,"wire"))return test_wire();
 if(!strcmp(op,"stage"))return system_update_stage(argv[3],argc>4 ? argv[4] : "9.1")?0:2;
 if(!strcmp(op,"apply"))return system_update_maintenance()?0:2;
 if(!strcmp(op,"rollback"))return system_update_rollback()?0:2;
 if(!strcmp(op,"keep"))return system_update_keep()?0:2;
 if(!strcmp(op,"install"))return app_package_install(argv[3])?0:2;
 if(!strcmp(op,"launch")){app_packages_scan();assert(app_package_installed(APP_SIGNAL));assert(app_process_open(APP_SIGNAL));for(int i=0;i<100&&!*app_process_frame();i++){usleep(10000);app_process_poll();}assert(strstr(app_process_frame(),"Schiebepuzzle"));assert(strstr(app_process_data_dir(),"/appdata/lumen.sliding-puzzle"));assert(app_process_send("{\"event\":\"component\",\"id\":\"restart\",\"gesture\":\"clicked\"}"));for(int i=0;i<30;i++){usleep(10000);app_process_poll();}assert(!*app_process_error());app_process_close();puts("PASS independent app process renders and handles input");return 0;}
 return 3;}
