#ifndef LUMEN_NOTIFICATIONS_H
#define LUMEN_NOTIFICATIONS_H
#include <stdint.h>
typedef struct {
    char text[2048];
    int duration_ms;
} NotificationRequest;
#endif
