#ifndef LUMEN_DISCOVER_H
#define LUMEN_DISCOVER_H
#define OPDS_LIMIT 128
typedef struct {
    char title[256], author[160], summary[768], url[2048], format[8];
    int acquisition;
} OPDSItem;
extern OPDSItem opds_items[OPDS_LIMIT];
extern int opds_count;
const char *discover_lookup_text(void);
const char *discover_lookup_title(void);
const char *discover_lookup_image(void);
const char *discover_lookup_image_name(void);
void discover_restore_lookup(const char*,const char*,const char*,const char*);
int discover_lookup(const char *query, int english);
int discover_parse_lookup(const char *path);
int discover_start(const char *url, const OPDSItem *book);
int discover_busy(void);
int discover_complete(void);
void discover_cancel(void);
int discover_parse(const char *path, const char *base);
const char *discover_error(void);
const char *discover_url(void);
#endif
