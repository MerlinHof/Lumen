/* App-owned module; assembled by apps/host/compose.h. */
static void extra_zone(void) {
    int z = options[OPT_ZONE];
    if (z < 0 || z > 4)
        z = 0;
    setenv("TZ", zone_values[z], 1);
    tzset();
}
