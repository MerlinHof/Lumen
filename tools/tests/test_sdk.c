#include "../sdk/lumen_app.h"
#include <assert.h>
#include <stdio.h>

int main(int argc,char **argv) {
    assert(argc==2 && !setenv("LUMEN_APP_DATA",argv[1],1));
    assert(wire_read("{\"nested\":{\"event\":\"wrong\"}, \"event\" : \"text\", \"n\":42, \"value\":\"A\\uD83D\\uDE00\"}"));
    assert(lumen_is("text") && !lumen_is("wrong") && lumen_number("n")==42);
    char output[20];size_t size;
    assert(lumen_string("value",output,sizeof(output)) && !strcmp(output,"A😀"));
    assert(!wire_read("{\"event\":\"text\", \"x\":12x}"));
    assert(!wire_read("{\"text\":\"\\uD800\"}"));
    assert(lumen_storage_write("documents/notes/one.txt","hello",5));
    assert(lumen_storage_read("documents/notes/one.txt",output,sizeof(output),&size));
    assert(size==5 && !memcmp(output,"hello",5));
    assert(!lumen_storage_write("documents/../escape","x",1));
    char link[2048];snprintf(link,sizeof(link),"%s/link",argv[1]);
    assert(!symlink("/tmp",link));
    assert(!lumen_storage_write("link/outside","x",1));
    assert(!lumen_storage_read("link/outside",output,sizeof(output),&size));
    assert(lumen_storage_delete("documents/notes/one.txt"));
    puts("PASS SDK JSON structure, Unicode, nested storage and traversal rejection.");
}
