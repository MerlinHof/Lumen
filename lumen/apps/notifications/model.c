#include "api.h"
#include "../../system/app_runtime.h"
#include <stdio.h>
static char message[2048];
static int64_t deadline;
static int captured;
void notification_show(const NotificationRequest *request, int64_t now) {
    if (!request)
        return;
    snprintf(message, sizeof(message), "%s", request->text);
    int duration = request->duration_ms > 0 ? request->duration_ms : 6000;
    if (duration > 60000)
        duration = 60000;
    deadline = now + duration;
}
void notification_dismiss(void) {
    message[0] = 0;
}
const char *notification_text(void) {
    return message;
}
int notification_tick(int64_t now) {
    if (*message && now >= deadline) {
        notification_dismiss();
        return 1;
    }
    return 0;
}
int notification_input(const Input *input, int visible) {
    int inside = input->x >= 19 && input->x < 577 && input->y >= 57 && input->y < 148;
    if (input->kind == INPUT_DOWN) {
        captured = visible && inside;
        return captured;
    }
    if (input->kind == INPUT_MOVE || input->kind == INPUT_HOLD)
        return captured || (visible && inside);
    if (input->kind == INPUT_TAP || input->kind == INPUT_SWIPE) {
        int consume = captured || (visible && inside);
        captured = 0;
        if (consume && input->kind == INPUT_TAP && input->x >= 515 && inside)
            notification_dismiss();
        return consume;
    }
    return 0;
}

static int receive_notification(const void *payload, size_t bytes, int64_t now) {
    if (bytes != sizeof(NotificationRequest))
        return 0;
    NotificationRequest copy = *(const NotificationRequest *)payload;
    copy.text[sizeof(copy.text) - 1] = 0;
    notification_show(&copy, now);
    return 1;
}
void notifications_register(void) {
    app_endpoint_register("notifications", receive_notification);
}
