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
 fputs("#!/bin/sh\nprintf '%s' '{\"tag_name\":\"v99.0\",\"assets\":[]}'\n",f);fclose(f);chmod("build/tests/fakebin/curl",0700);
 char *old=strdup(getenv("PATH")),path[8192],cwd[4096];assert(getcwd(cwd,sizeof(cwd)));
 snprintf(path,sizeof(path),"%s/build/tests/fakebin:%s",cwd,old);setenv("PATH",path,1);
 assert(release_set_beta(0) && !release_beta_enabled());
 assert(release_start(0));wait_done();assert(release_state()==3&&!strcmp(release_version(),"v99.0")&&release_checked()>0);
 assert(release_start(1));wait_done();assert(release_state()==-1&&strstr(release_error(),"USB"));
 assert(access("build/tests/.update/approved",F_OK));
 f=fopen("build/tests/fakebin/curl","w");assert(f);
 fputs("#!/bin/sh\ncase \"$*\" in *tags/lumen-beta*) printf '%s' "
       "'{\"tag_name\":\"lumen-beta\",\"assets\":[{\"name\":\"LUMEN-99.2.wireless.json\",\"browser_download_url\":\"https://github.com/MerlinHof/Lumen/releases/download/lumen-beta/LUMEN-99.2.wireless.json\"}]}';; "
       "*) printf '%s' '{\"tag_name\":\"v99.1\",\"assets\":[]}';; esac\n",f);
 fclose(f);
 assert(release_set_beta(1) && release_beta_enabled());
 assert(release_start(0));wait_done();assert(release_state()==3 && !strcmp(release_version(),"v99.2"));
 assert(release_set_beta(0) && !release_beta_enabled());
 assert(release_start(0));wait_done();assert(release_state()==3 && !strcmp(release_version(),"v99.1"));
 release_cancel();setenv("PATH",old,1);free(old);
 puts("PASS GitHub release discovery and incompatible-resource update refusal.");
}
