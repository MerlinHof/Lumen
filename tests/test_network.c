#include "../src/network.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    assert(network_scan());
    assert(network_results());
    assert(wifi_count == 2);
    assert(!strcmp(wifi_networks[0].ssid, "Testnetz"));
    assert(wifi_networks[0].saved);
    assert(!network_connect(0, "short"));
    assert(network_connect(0, "valid-password"));
    assert(network_connect(1, ""));
    char status[128];
    network_status(status, sizeof(status));
    assert(!strcmp(status, "Testnetz"));
    assert(network_disconnect());
    puts("PASS: network parsing, saved profile, password validation, protected/open connection and "
         "disconnect (mock backend).");
}
