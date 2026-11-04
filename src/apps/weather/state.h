/* App-owned state; stable on-disk schemas are intentionally preserved. */
static char weather_city[256]="Rosenheim";
static int weather_snapshot_valid;static char *weather_snapshot;
