#include "../src/amazon.h"
#include "../src/engine.h"
#include <assert.h>
#include <stdio.h>
#include <sys/stat.h>
int main(int argc,char **argv){assert(argc==4);data_root=argv[1];assert(engine_init());assert(engine_open(argv[2],22,145,12));ReaderPosition p;assert(engine_position(4,&p));int newpage=engine_relayout(30,160,15,4);assert(newpage>=0&&newpage<engine_count());ReaderPosition q;assert(engine_position(newpage,&q));assert(p.chapter==q.chapter);assert(engine_restore(&p)==newpage);struct stat st;assert(!stat(argv[3],&st));Book b={.id=123,.size=st.st_size,.modified=st.st_mtime};snprintf(b.path,sizeof(b.path),"%s",argv[3]);char path[2048];assert(amazon_prepare(&b,path,sizeof(path)));assert(engine_open(path,22,145,12));assert(engine_count()>0);engine_shutdown();puts("PASS: chapter-aware reflow anchor and unencrypted MOBI conversion/rendering.");}
