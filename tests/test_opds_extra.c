#include "../src/discover.h"
#include "../src/library.h"
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
static volatile sig_atomic_t ready;
static void notified(int s){(void)s;ready=1;}
int main(int argc,char **argv){assert(argc==4);library_root=argv[1];data_root=argv[2];signal(SIGUSR1,notified);assert(discover_parse(argv[3],"http://127.0.0.1:18081/feed.xml"));assert(opds_count==1);assert(opds_items[0].acquisition);assert(!strcmp(opds_items[0].url,"http://127.0.0.1:18081/book.epub"));assert(discover_start("http://127.0.0.1:18081/feed.xml",NULL));for(int i=0;i<100&&!ready;i++)usleep(100000);assert(ready);assert(discover_complete()==1);ready=0;OPDSItem book=opds_items[0];assert(discover_start(book.url,&book));for(int i=0;i<100&&!ready;i++)usleep(100000);assert(ready);assert(discover_complete()==2);assert(!discover_start(book.url,&book));puts("PASS: OPDS parsing, relative acquisition URL, asynchronous catalogue/download, duplicate protection.");}
