#ifndef LUMEN_NOTIFICATIONS_H
#define LUMEN_NOTIFICATIONS_H
#include "../../system/device.h"
#include <stdint.h>
typedef struct {
    char text[2048];
    int duration_ms;
} NotificationRequest;
void notifications_register(void);
void notification_show(const NotificationRequest *request, int64_t now);
void notification_dismiss(void);
const char *notification_text(void);
int notification_tick(int64_t now);
/* A captured gesture remains captured after leaving the box or dismissal. */
int notification_input(const Input *input, int visible);
#endif
