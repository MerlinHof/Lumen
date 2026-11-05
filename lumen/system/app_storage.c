#include "app_storage.h"
#include "library.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int state_path(char path[2048], const char *name) {
    if (!name || !*name) return 0;
    for (const char *part = name; ; ) {
        size_t length = strcspn(part, "/");
        if (!length || (length == 1 && part[0] == '.') ||
            (length == 2 && part[0] == '.' && part[1] == '.')) return 0;
        if (!part[length]) break;
        part += length + 1;
    }
    int length = snprintf(path, 2048, "%s/%s", data_root, name);
    return length > 0 && length < 2048;
}
int app_state_save(const char *name, const void *data, size_t bytes) {
    char path[2048];
    uint32_t header[2] = {0x4c554d16, (uint32_t)bytes};
    if (!data || !bytes || bytes > UINT32_MAX || bytes > SIZE_MAX - sizeof(header) ||
        !state_path(path, name)) return 0;
    unsigned char *all = malloc(sizeof(header) + bytes);
    if (!all) return 0;
    memcpy(all, header, sizeof(header));
    memcpy(all + sizeof(header), data, bytes);
    int ok = atomic_write(path, all, sizeof(header) + bytes);
    free(all);
    return ok;
}
int app_state_load(const char *name, void *data, size_t bytes) {
    char path[2048];
    if (!data || !bytes || bytes > UINT32_MAX || !state_path(path, name)) return 0;
    FILE *file = fopen(path, "rb");
    if (!file) return 0;
    uint32_t header[2];
    int ok = 0;
    if (fread(header, sizeof(header), 1, file) == 1 &&
        header[0] == 0x4c554d16 && header[1] == bytes) {
        void *scratch = malloc(bytes);
        ok = scratch && fread(scratch, 1, bytes, file) == bytes &&
             fgetc(file) == EOF && !ferror(file);
        if (ok) memcpy(data, scratch, bytes);
        free(scratch);
    }
    fclose(file);
    return ok;
}
