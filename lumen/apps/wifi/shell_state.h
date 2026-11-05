/* State owned by the wifi app in the legacy view host. */
typedef struct {
    int wifi_enabled;
    int wifi_page;
    int wifi_selected;
    int wifi_scanning;
    int wifi_exit;
    char wifi_ssid[128];
} WifiUi;
static WifiUi wifi_ui = {
    .wifi_enabled = 1,
    .wifi_selected = -1,
};
