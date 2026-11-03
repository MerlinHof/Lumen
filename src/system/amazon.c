#include "../i18n.h"
#define _GNU_SOURCE
#include "../amazon.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/resource.h>
static const char *tool(void) {
    const char *p = getenv("LUMEN_MOBITOOL");
    return p ? p : "/mnt/us/lumen/bin/mobitool";
}
int amazon_metadata(const char *path, char *title, size_t nt, char *author, size_t na,
                    int *encrypted) {
    int fds[2];
    if (pipe(fds))
        return 0;
    pid_t p = fork();
    if (!p) {
        close(fds[0]);
        dup2(fds[1], 1);
        dup2(fds[1], 2);
        close(fds[1]);
        execl(tool(), "mobitool", path, NULL);
        _exit(127);
    }
    close(fds[1]);
    FILE *f = fdopen(fds[0], "r");
    int found = 0;
    char line[2048];
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\r\n")] = 0;
            if (!strncmp(line, "Title: ", 7)) {
                snprintf(title, nt, "%s", line + 7);
                found = 1;
            } else if (!strncmp(line, "Author: ", 8))
                snprintf(author, na, "%s", line + 8);
            else if (strstr(line, "Document is encrypted"))
                *encrypted = 1;
        }
        fclose(f);
    } else
        close(fds[0]);
    if (p > 0)
        waitpid(p, NULL, 0);
    return found;
}
int amazon_prepare(const Book *book, char *path, size_t size) {
    char parent[2048], dir[2048];
    snprintf(parent, sizeof(parent), "%s/converted", data_root);
    mkdir(parent, 0700);
    snprintf(dir, sizeof(dir), "%s/%016llx", parent, (unsigned long long)book->id);
    mkdir(dir, 0700);
    char stamp[2048];
    snprintf(stamp, sizeof(stamp), "%s/source", dir);
    unsigned long long bytes = 0;
    long long modified = 0;
    FILE *sf = fopen(stamp, "r");
    if (sf) {
        fscanf(sf, "%llu %lld", &bytes, &modified);
        fclose(sf);
    }
    int valid = bytes == book->size && modified == book->modified;
    if (valid) {
        DIR *d = opendir(dir);
        if (d) {
            struct dirent *e;
            while ((e = readdir(d)))
                if (strlen(e->d_name) > 5 && !strcmp(e->d_name + strlen(e->d_name) - 5, ".epub")) {
                    snprintf(path, size, "%s/%s", dir, e->d_name);
                    closedir(d);
                    return 1;
                }
            closedir(d);
        }
    }
    pid_t p = fork();
    if (!p) {
        struct rlimit mem = {96 * 1024 * 1024, 96 * 1024 * 1024}, cpu = {30, 30};
        setrlimit(RLIMIT_AS, &mem);
        setrlimit(RLIMIT_CPU, &cpu);
        char log[2048];
        snprintf(log, sizeof(log), "%s/conversion.log", dir);
        int fd = open(log, O_CREAT | O_TRUNC | O_WRONLY, 0600);
        if (fd >= 0) {
            dup2(fd, 1);
            dup2(fd, 2);
            close(fd);
        }
        execl(tool(), "mobitool", "-e", "-o", dir, book->path, NULL);
        _exit(127);
    }
    int status = 0;
    if (p <= 0)
        return 0;
    while (waitpid(p, &status, 0) < 0) {
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status))
        return 0;
    DIR *d = opendir(dir);
    if (!d)
        return 0;
    int found = 0;
    struct dirent *e;
    while ((e = readdir(d)))
        if (strlen(e->d_name) > 5 && !strcmp(e->d_name + strlen(e->d_name) - 5, ".epub")) {
            snprintf(path, size, "%s/%s", dir, e->d_name);
            found = 1;
            break;
        }
    closedir(d);
    if (found) {
        char buf[100];
        int n = snprintf(buf, sizeof(buf), "%llu %lld\n", (unsigned long long)book->size,
                         (long long)book->modified);
        atomic_write(stamp, buf, (size_t)n);
    }
    return found;
}
int amazon_reader_request(const char *path) {
    if (strncmp(path, "/mnt/us/documents/", 18) || strchr(path, '\n'))
        return 0;
    char uri[6000] = "app://com.lab126.booklet.reader";
    size_t n = strlen(uri);
    for (const unsigned char *p = (const unsigned char *)path; *p && n + 4 < sizeof(uri); p++) {
        unsigned char c = *p;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
            c == '/' || c == '-' || c == '_' || c == '.')
            uri[n++] = (char)c;
        else
            n += (size_t)snprintf(uri + n, sizeof(uri) - n, "%%%02X", c);
    }
    uri[n] = 0;
    char output[2048];
    snprintf(output, sizeof(output), "%s/../reader-request.txt", data_root);
    return atomic_write(output, uri, n);
}
