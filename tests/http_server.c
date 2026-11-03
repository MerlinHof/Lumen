#include "../src/connect.h"
#include "../src/engine.h"
#include "../src/library.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
int main(){library_root="build/tests/http-books";data_root="build/tests/http-data";mkdir(library_root,0700);mkdir(data_root,0700);engine_init();if(!connect_start(18889,1))return 1;printf("%s\n%s\n",connect_address(),connect_code());fflush(stdout);char c;read(0,&c,1);connect_stop();engine_shutdown();}
