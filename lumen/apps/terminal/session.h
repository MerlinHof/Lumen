#ifndef LUMEN_TERMINAL_SESSION_H
#define LUMEN_TERMINAL_SESSION_H
#include <stddef.h>
#include <sys/types.h>

typedef struct {
    pid_t pid;
    int input, output;
    unsigned sequence;
} TerminalSession;

void terminal_session_close(TerminalSession *session);
int terminal_session_command(TerminalSession *session, const char *directory,
                             const char *command, char *out, size_t capacity);
#endif
