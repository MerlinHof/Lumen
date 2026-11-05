/* App-owned state; stable on-disk schemas are intentionally preserved. */
static char ha_url[512],ha_token[512],ha_favorites[128][160];
static int ha_favorite_count,ha_page,ha_select,ha_count;
typedef struct {char id[160],name[192],state[64],unit[32];int sensor;} HAEntity;
static HAEntity ha_entities[512];
