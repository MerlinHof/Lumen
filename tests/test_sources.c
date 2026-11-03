#include "../src/discover.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(int argc, char **argv) {
    assert(argc == 3);
    assert(discover_parse(argv[1], "https://archive.org/advancedsearch.php?q=philosophy&rows=50&output=xml"));
    assert(opds_count == 6 && !opds_items[0].acquisition);
    assert(strstr(opds_items[0].url, "&page=2"));
    assert(strstr(opds_items[1].url, "_files.xml"));
    assert(discover_parse(argv[2], "https://archive.org/download/sample/sample_files.xml"));
    assert(opds_count == 1 && opds_items[0].acquisition);
    assert(!strcmp(opds_items[0].format, "pdf"));
    assert(strstr(opds_items[0].url, "https://archive.org/download/sample/Prime%20Matter"));
    puts("PASS: real Archive search XML, pagination, file lists and escaped PDF acquisition.");
}
