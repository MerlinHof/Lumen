#include "../sdk/lumen_app.h"
#include "../sdk/lumen_http.h"
#include <assert.h>
#include <stdio.h>

static int check_entry(const char *name,int directory,void *context) {
    int *count=context;
    assert(!strcmp(name,"one.txt") && !directory);
    (*count)++;
    return 0;
}

static void check_http(const char *directory) {
    char curl[2048];
    snprintf(curl,sizeof(curl),"%s/curl",directory);
    FILE *fixture=fopen(curl,"w");
    assert(fixture);
    fputs("#!/bin/sh\n"
          "case \"$*\" in *fixture-bearer*) exit 2;; esac\n"
          "for argument do url=$argument; done\n"
          "case \"$url\" in\n"
          " */exact) printf %s 12345678;;\n"
          " */large) printf %s 123456789;;\n"
          " */binary) printf 'A\\000Z';;\n"
          " */json) cfg=$(/bin/cat); case \"$cfg\" in *'Authorization: Bearer fixture-bearer'*) printf %s '{}' ;; *) exit 3;; esac;;\n"
          " *) exit 4;;\n"
          "esac\n",fixture);
    assert(!fclose(fixture) && !chmod(curl,0700));
    char *path=strdup(getenv("PATH"));
    assert(path && !setenv("PATH",directory,1));
    char text[9];
    assert(lumen_https_get("https://fixture.invalid/exact",text,sizeof(text)) && !strcmp(text,"12345678"));
    assert(!lumen_https_get("https://fixture.invalid/large",text,sizeof(text)) && !*text);
    assert(!lumen_https_get("https://fixture.invalid/binary",text,sizeof(text)) && !*text);
    assert(!lumen_https_get("http://fixture.invalid/exact",text,sizeof(text)));
    LumenHttpRequest post={.url="https://fixture.invalid/json",.bearer="fixture-bearer",.json="{\"v\":1}"};
    assert(lumen_http_text(post,text,sizeof(text)) && !strcmp(text,"{}"));
    post.redirects=1;
    assert(!lumen_http_text(post,text,sizeof(text)));
    FILE *file=tmpfile();
    assert(file && lumen_http_download((LumenHttpRequest){.url="https://fixture.invalid/binary"},fileno(file),3));
    assert(fread(text,1,3,file)==3 && !memcmp(text,"A\0Z",3));
    fclose(file);
    assert(!setenv("PATH",path,1));
    free(path);
    assert(lumen_url_encode("Ä &",text,sizeof(text))==0 && !*text);
    char encoded[32];
    assert(lumen_url_encode("Ä &",encoded,sizeof(encoded)) && !strcmp(encoded,"%C3%84%20%26"));
    puts("PASS SDK HTTP limits, binary files, URL encoding, and bearer isolation.");
}

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
    assert(lumen_storage_exists("documents/notes/one.txt"));
    assert(!lumen_storage_exists("link") && !lumen_storage_list("link",check_entry,&(int){0}));
    assert(lumen_storage_mkdir("documents/drafts/deep") && lumen_storage_mkdir("documents/drafts/deep"));
    assert(lumen_storage_exists("documents/drafts/deep"));
    assert(!lumen_storage_mkdir("documents/notes/one.txt"));
    assert(!lumen_storage_mkdir("link/new") && !lumen_storage_mkdir("documents/../escape"));
    int count=0;
    assert(lumen_storage_list("documents/notes",check_entry,&count) && count==1);
    assert(lumen_storage_delete("documents/notes/one.txt"));
    assert(!lumen_storage_exists("documents/notes/one.txt"));
    assert(lumen_storage_list("documents/notes",check_entry,&count) && count==1);
    check_http(argv[1]);
    puts("PASS SDK JSON structure, Unicode, nested storage and traversal rejection.");
}
