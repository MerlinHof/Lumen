#include "../../lumen/system/release_update.h"
#include "../../lumen/system/library.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static void wait_done(void){for(int i=0;i<400&&release_state()==1;i++){usleep(10000);release_poll();}assert(release_state()!=1);}
int main(void){
 data_root="build/tests/release-client-data";mkdir(data_root,0700);mkdir("build/tests/fakebin",0700);
 FILE*f=fopen("build/tests/fakebin/curl","w");assert(f);
 fputs("#!/bin/sh\nout=''\nwhile [ $# -gt 0 ]; do\nif [ \"$1\" = --output ]; then shift; out=$1; fi\nshift\ndone\nprintf '%s' '{\"tag_name\":\"v99.0\",\"assets\":[]}' > \"$out\"\n",f);fclose(f);chmod("build/tests/fakebin/curl",0700);
 char *old=strdup(getenv("PATH")),path[8192],cwd[4096];assert(getcwd(cwd,sizeof(cwd)));
 snprintf(path,sizeof(path),"%s/build/tests/fakebin:%s",cwd,old);setenv("PATH",path,1);
 assert(release_start(0));wait_done();assert(release_state()==3&&!strcmp(release_version(),"v99.0")&&release_checked()>0);
 assert(release_start(1));wait_done();assert(release_state()==-1&&strstr(release_error(),"USB"));
 assert(access("build/tests/.update/approved",F_OK));
 release_cancel();setenv("PATH",old,1);free(old);
 puts("PASS GitHub release discovery and incompatible-resource update refusal.");
}
