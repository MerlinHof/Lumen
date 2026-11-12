/* App-owned module; assembled by apps/host/compose.h. */
static void wifi_open(void) {
    wifi_ui.wifi_page = 0;
    wifi_ui.wifi_exit = 0;
    wifi_ui.wifi_enabled = device_wifi_enabled();
    network_status(wifi_ui.wifi_ssid, sizeof(wifi_ui.wifi_ssid));
    if (*wifi_ui.wifi_ssid)
        wifi_ui.wifi_enabled = 1;
    if (wifi_ui.wifi_enabled) {
        wifi_ui.wifi_scanning = network_scan();
        network_results();
    } else
        wifi_count = 0;
}
