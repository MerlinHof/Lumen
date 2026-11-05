/* App-owned module; assembled by apps/host/compose.h. */
static void ha_parse(void) {
    ha_count = 0;
    char *s = service_read("ha-states.json", 2 * 1024 * 1024);
    if (!s)
        return;
    JsonToken *tokens = calloc(32000, sizeof(*tokens));
    Json j;
    if (tokens && json_parse(&j, s, tokens, 32000)) {
        for (int n = 0; n < j.tokens[0].count && ha_count < 512; n++) {
            int obj = json_at(&j, 0, n);
            HAEntity v = {0};
            json_string(&j, json_get(&j, obj, "entity_id"), v.id, sizeof(v.id));
            if (strncmp(v.id, "light.", 6) && strncmp(v.id, "switch.", 7) &&
                strncmp(v.id, "input_boolean.", 14) && strncmp(v.id, "fan.", 4) &&
                strncmp(v.id, "sensor.", 7) && strncmp(v.id, "binary_sensor.", 14))
                continue;
            json_string(&j, json_get(&j, obj, "state"), v.state, sizeof(v.state));
            json_string(&j, json_get(&j, json_get(&j, obj, "attributes"), "friendly_name"), v.name,
                        sizeof(v.name));
            if (!*v.name)
                snprintf(v.name, sizeof(v.name), "%s", v.id);
            v.sensor = !strncmp(v.id, "sensor.", 7) || !strncmp(v.id, "binary_sensor.", 14);
            json_string(&j, json_get(&j, json_get(&j, obj, "attributes"), "unit_of_measurement"),
                        v.unit, sizeof(v.unit));
            ha_entities[ha_count++] = v;
        }
    }
    free(tokens);
    free(s);
}
static int ha_favorite(const char *id) {
    for (int i = 0; i < ha_favorite_count; i++)
        if (!strcmp(ha_favorites[i], id))
            return i;
    return -1;
}
#include "dashboard.h"
