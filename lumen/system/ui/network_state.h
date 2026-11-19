static struct {
    int enabled,scanning;
    char ssid[128];
} wifi_ui;

static void wifi_open(void) {
    wifi_ui.enabled=device_wifi_enabled();
    network_status(wifi_ui.ssid,sizeof(wifi_ui.ssid));
    if(*wifi_ui.ssid)wifi_ui.enabled=1;
    if(wifi_ui.enabled) {
        wifi_ui.scanning=network_scan();
        network_results();
    } else wifi_count=0;
}
