#ifndef LUMEN_WIRELESS_UPDATE_H
#define LUMEN_WIRELESS_UPDATE_H
#include <stddef.h>
#include <stdint.h>
#define UPDATE_DIGEST_SIZE 65
int update_receive(int fd, uint64_t length, const char *initial, size_t initial_length,
                   char digest[UPDATE_DIGEST_SIZE]);
int update_fingerprint(char digest[UPDATE_DIGEST_SIZE]);
int update_approve(const char *digest);
int update_trial(void);
int update_keep(void);
int update_apply(void);
#endif
