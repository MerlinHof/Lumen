#ifndef LUMEN_CONNECT_H
#define LUMEN_CONNECT_H
int connect_start(int port, int timeout_minutes);
void connect_stop(void);
int connect_running(void);
const char *connect_address(void);
const char *connect_code(void);
const char *connect_error(void);
#endif
