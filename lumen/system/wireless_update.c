#define _GNU_SOURCE
#include "wireless_update.h"
#include "library.h"
#include "mupdf/fitz/crypt.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

enum { UPDATE_MIN = 4096, UPDATE_MAX = 64*1024*1024 };
static void update_path(char path[2048], const char *leaf) {
    snprintf(path,2048,"%s/../.update/%s",data_root,leaf);
}
static int read_digest(const char *leaf, char digest[UPDATE_DIGEST_SIZE]) {
    char path[2048];
    update_path(path,leaf);
    FILE *f = fopen(path,"rb");
    if (!f) return 0;
    digest[64] = 0;
    int ok = fread(digest,1,64,f) == 64 && fgetc(f) == EOF &&
             strspn(digest,"0123456789abcdef") == 64;
    fclose(f);
    return ok;
}
static int save_digest(const char *leaf, const char *digest) {
    char path[2048];
    update_path(path,leaf);
    return atomic_write(path,digest,64);
}
static void digest_hex(fz_sha256 *hash, char out[UPDATE_DIGEST_SIZE]) {
    unsigned char bytes[32];
    fz_sha256_final(hash,bytes);
    for (size_t i=0; i<sizeof(bytes); i++)
        snprintf(out+2*i,3,"%02x",bytes[i]);
}
static int kindle_executable(const unsigned char *head) {
    return !memcmp(head,"\177ELF\1\1\1",7) && head[16] == 2 && head[17] == 0 &&
           head[18] == 40 && head[19] == 0;
}
int update_trial(void) {
    char previous[2048];
    update_path(previous,"previous");
    return access(previous,F_OK) == 0;
}
int update_fingerprint(char digest[UPDATE_DIGEST_SIZE]) {
    char incoming[2048];
    update_path(incoming,"incoming");
    return access(incoming,R_OK) == 0 && read_digest("received",digest);
}
int update_approve(const char *digest) {
    char current[UPDATE_DIGEST_SIZE];
    return !update_trial() && update_fingerprint(current) &&
           !strcmp(current,digest) && save_digest("approved",digest);
}
int update_keep(void) {
    char previous[2048];
    update_path(previous,"previous");
    return unlink(previous) == 0;
}
int update_receive(int fd, uint64_t length, const char *initial, size_t initial_length,
                   char digest[UPDATE_DIGEST_SIZE]) {
    char directory[2048], incoming[2048], temporary[2048], approved[2048];
    update_path(incoming,"incoming");
    update_path(temporary,"upload");
    update_path(approved,"approved");
    if (length < UPDATE_MIN || length > UPDATE_MAX || initial_length > length) return 400;
    if (update_trial() || access(approved,F_OK) == 0) return 409;
    struct statvfs space;
    if (statvfs(data_root,&space) ||
        (uint64_t)space.f_bavail*space.f_frsize < length + 8*1024*1024) return 507;
    snprintf(directory,sizeof(directory),"%s/../.update",data_root);
    if (mkdir(directory,0700) && errno != EEXIST) return 500;
    int out = open(temporary,O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW,0600);
    if (out < 0) return 500;
    FILE *file = fdopen(out,"wb");
    if (!file) { close(out); unlink(temporary); return 500; }
    unsigned char head[20], buf[32768];
    size_t header_length = 0;
    fz_sha256 hash;
    fz_sha256_init(&hash);
    int status = 201;
    while (length) {
        const unsigned char *data = (const unsigned char *)initial;
        size_t n = initial_length;
        if (!n) {
            size_t limit = length < sizeof(buf) ? (size_t)length : sizeof(buf);
            ssize_t received = recv(fd,buf,limit,0);
            if (received < 0 && errno == EINTR) continue;
            if (received <= 0) { status=400; break; }
            n=(size_t)received;
            data=buf;
        }
        if (header_length < sizeof(head)) {
            size_t take=sizeof(head)-header_length;
            if (take > n) take=n;
            memcpy(head+header_length,data,take);
            header_length+=take;
            if (header_length == sizeof(head) && !kindle_executable(head)) {
                status=400;
                break;
            }
        }
        if (fwrite(data,1,n,file) != n) { status=500; break; }
        fz_sha256_update(&hash,data,n);
        length-=n;
        initial_length=0;
    }
    if (status == 201 && (fflush(file) || fsync(out))) status=500;
    if (fclose(file) && status == 201) status=500;
    if (status == 201) {
        digest_hex(&hash,digest);
        if (rename(temporary,incoming) || !save_digest("received",digest)) status=500;
    }
    if (status != 201) unlink(temporary);
    return status;
}
int update_apply(void) {
    char current[2048], previous[2048], incoming[2048], approved[2048], received[2048];
    char expected[UPDATE_DIGEST_SIZE], actual[UPDATE_DIGEST_SIZE];
    update_path(previous,"previous");
    update_path(incoming,"incoming");
    update_path(approved,"approved");
    update_path(received,"received");
    snprintf(current,sizeof(current),"%s/../bin/lumen",data_root);
    if (update_trial() || !read_digest("approved",expected)) return 0;
    FILE *file = fopen(incoming,"rb");
    if (!file) return 0;
    fz_sha256 hash;
    fz_sha256_init(&hash);
    unsigned char buf[32768];
    size_t n, total=0;
    int valid=1;
    while ((n=fread(buf,1,sizeof(buf),file))) {
        if (!total && (n < 20 || !kindle_executable(buf))) { valid=0; break; }
        total+=n;
        if (total > UPDATE_MAX) { valid=0; break; }
        fz_sha256_update(&hash,buf,n);
    }
    valid = valid && !ferror(file) && total >= UPDATE_MIN;
    fclose(file);
    digest_hex(&hash,actual);
    if (!valid || strcmp(actual,expected) || chmod(incoming,0700)) return 0;
    if (unlink(approved) || rename(current,previous)) return 0;
    sync();
    if (rename(incoming,current)) {
        rename(previous,current);
        sync();
        return 0;
    }
    unlink(received);
    sync();
    return 1;
}
