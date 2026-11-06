#include "../../lumen/system/connect.h"
#include "../../lumen/system/engine.h"
#include "../../lumen/system/library.h"
#include "../../lumen/system/wireless_update.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

int main(void) {
    library_root = "build/tests/http-sandbox/books";
    data_root = "build/tests/http-sandbox/data";
    mkdir(library_root,0700);
    mkdir(data_root,0700);
    engine_init();
    if (!connect_start(18889,1)) return 1;
    printf("%s\n%s\n",connect_address(),connect_code());
    fflush(stdout);
    char command=0;
    if (read(0,&command,1) != 1) command=0;
    connect_stop();
    engine_shutdown();
    if (command == 'u') {
        char digest[UPDATE_DIGEST_SIZE];
        if (!update_fingerprint(digest) || !update_approve(digest) || !update_apply() || !update_trial())
            return 2;
    }
    return 0;
}
