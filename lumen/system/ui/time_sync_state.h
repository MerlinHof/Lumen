/* App-owned state; stable on-disk schemas are intentionally preserved. */
static time_t last_sync, last_time_success;
static int time_year, time_month, time_day, time_hour, time_minute;
static const char *zone_names[] = {"Berlin / Europa", "UTC", "London", "New York", "Tokyo"};
static const char *zone_values[] = {"CET-1CEST,M3.5.0,M10.5.0/3", "UTC0",
                                    "GMT0BST,M3.5.0/1,M10.5.0", "EST5EDT,M3.2.0,M11.1.0", "JST-9"};
