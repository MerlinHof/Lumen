/* App-owned module; assembled by apps/host/compose.h. */
#include "../../system/markdown.h"
static char model_ids17[512][160];
static const char *model_labels17[512];
static int model_count17, models_loaded17;
static void models_parse17(void) {
    model_count17 = 0;
    char *s = service_read("llm-models17.json", 4194304);
    if (!s)
        return;
    Json j;
    JsonToken *t = calloc(50000, sizeof(*t));
    if (t && json_parse(&j, s, t, 50000)) {
        int a = json_get(&j, 0, "data");
        for (int i = 0; i < 512; i++) {
            int o = json_at(&j, a, i);
            if (o < 0)
                break;
            json_string(&j, json_get(&j, o, "id"), model_ids17[model_count17], 160);
            if (*model_ids17[model_count17])
                model_count17++;
        }
    }
    free(t);
    free(s);
    for (int i = 0; i < model_count17; i++)
        for (int k = i + 1; k < model_count17; k++)
            if (strcmp(model_ids17[k], model_ids17[i]) < 0) {
                char b[160];
                memcpy(b, model_ids17[i], 160);
                memcpy(model_ids17[i], model_ids17[k], 160);
                memcpy(model_ids17[k], b, 160);
            }
    for (int i = 0; i < model_count17; i++)
        model_labels17[i] = model_ids17[i];
}
