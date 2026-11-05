/* State owned by the status bar app in the legacy view host. */
typedef struct {
    int status_wifi_x;
    int status_battery_x;
    int status_battery_right;
    int status_date_x, status_date_w, status_clock_x, status_clock_w;
    int status_timer_x;
    int status_timer_width;
    char battery[24];
    char wifi[32];
    time_t status_time;
    int battery_charging;
    time_t charging_checked;
} StatusUi;
static StatusUi status_ui = {
    .status_wifi_x = -1,
    .status_battery_x = -1,
    .status_timer_x = -1,
};
