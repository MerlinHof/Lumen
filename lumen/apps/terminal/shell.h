#ifndef LUMEN_TERMINAL_SHELL_H
#define LUMEN_TERMINAL_SHELL_H
#include <stddef.h>
#define TERMINAL_ACCESS "/tmp/lumen-terminal-access"
#define TERMINAL_WINDOW 600
int terminal_access(void);
int terminal_exec(const char *command, const char *directory, char *out, size_t capacity);
#endif
