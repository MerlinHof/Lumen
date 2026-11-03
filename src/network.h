#ifndef LUMEN_NETWORK_H
#define LUMEN_NETWORK_H
#define WIFI_LIMIT 32
typedef struct {
    char ssid[128], flags[160];
    int signal, saved, network_id;
} WifiNetwork;
extern WifiNetwork wifi_networks[WIFI_LIMIT];
extern int wifi_count;
int network_scan(void);
int network_results(void);
int network_connect(int index, const char *password);
int network_disconnect(void);
int network_forget(int index);
void network_status(char *ssid, int size);
const char *network_error(void);
#endif
