typedef struct {
    int category;
    char title[256], body[2300], url[1024], publisher[80], published[80], image[128];
} NewsArticle;
typedef struct {
    uint32_t magic, version, date, topics;
    int count;
    NewsArticle article[10];
} Newspaper;
static Newspaper newspaper;
static int news_date(void) {
    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);
    return (t.tm_year + 1900) * 10000 + (t.tm_mon + 1) * 100 + t.tm_mday;
}
static int news_load(int date) {
    const char *root=getenv("LUMEN_LEGACY_DATA");
    if(!root || !*root)return 0;
    char name[80];
    snprintf(name,sizeof(name),"news/%08d.bin",date);
    Newspaper *paper = malloc(sizeof(*paper));
    size_t length;
    int ok=paper && lumen_storage_read_at(root,name,paper,sizeof(*paper),&length) && length==sizeof(*paper);
    if (ok)
        ok = paper->magic == 0x4c4e4557 && paper->version == 1 && paper->date == (unsigned)date &&
             paper->count > 0 && paper->count <= 10;
    if (ok) {
        for (int i = 0; i < paper->count; i++) {
            NewsArticle *a = &paper->article[i];
            if (a->category < 0 || a->category > 5)
                ok = 0;
            a->title[255] = a->body[2299] = a->url[1023] = a->publisher[79] = a->published[79] =
                a->image[127] = 0;
        }
        if (ok)
            newspaper = *paper;
    }
    free(paper);
    return ok;
}
