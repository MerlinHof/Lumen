#ifndef LUMEN_DISCOVER_H
#define LUMEN_DISCOVER_H
#define OPDS_LIMIT 128
typedef struct {
    char title[256], author[160], summary[768], url[2048], format[8];
    int acquisition;
} OPDSItem;
extern OPDSItem opds_items[OPDS_LIMIT];
extern int opds_count;
int discover_start(const char *url, const OPDSItem *book);
int discover_busy(void);
int discover_complete(void);
void discover_cancel(void);
int discover_parse(const char *path, const char *base);
const char *discover_error(void);
const char *discover_url(void);
#endif
