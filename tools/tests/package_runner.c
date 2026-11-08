#include "../../lumen/system/packages.h"
#include "../../lumen/system/system_update.h"
#include "../../lumen/system/app_packages.h"
#include "../../lumen/system/library.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
int main(int argc,char**argv){assert(argc>=3);data_root=argv[1];const char*op=argv[2];
 if(!strcmp(op,"stage"))return system_update_stage(argv[3],"9.1")?0:2;
 if(!strcmp(op,"apply"))return system_update_maintenance()?0:2;
 if(!strcmp(op,"keep"))return system_update_keep()?0:2;
 if(!strcmp(op,"install"))return app_package_install(argv[3])?0:2;
 if(!strcmp(op,"launch")){app_packages_scan();assert(app_package_installed(APP_SIGNAL));assert(app_process_open(APP_SIGNAL));for(int i=0;i<100&&!*app_process_frame();i++){usleep(10000);app_process_poll();}assert(strstr(app_process_frame(),"Schiebepuzzle"));assert(app_process_send("{\"event\":\"input\",\"kind\":1,\"x\":550,\"y\":90}"));for(int i=0;i<30;i++){usleep(10000);app_process_poll();}assert(!*app_process_error());app_process_close();puts("PASS independent app process renders and handles input");return 0;}
 return 3;}
