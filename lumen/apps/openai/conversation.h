/* Local chat transcript and a bounded paper-like conversation viewport. */
#define CHAT_LIMIT 64
static struct {
    int role;
    char *text;
} chat_messages[CHAT_LIMIT];
static int chat_count,chat_page;
static unsigned long long chat_id;
static struct {
    unsigned long long id;
    time_t modified;
    char title[100];
} chat_archive[256];
static int chat_archive_count, chat_archive_page;
static char chat_title[100];
static void chat_path(char *out, size_t size, unsigned long long id) {
    snprintf(out,size,"%s/%016llx.chat",data_root,id);
}
static void chat_clear(void) {
    for (int i = 0; i < chat_count; i++) {
        free(chat_messages[i].text);
        chat_messages[i].text = NULL;
    }
    chat_count = 0;
    chat_id = 0;
    chat_page = 0;
    chat_title[0] = 0;
}
static void chat_save(void) {
    if (!chat_id || !chat_count)
        return;
    char *bytes = NULL;
    size_t size = 0;
    FILE *f = open_memstream(&bytes, &size);
    if (!f)
        return;
    fwrite("LCHAT1\n", 1, 7, f);
    uint32_t count = chat_count;
    fwrite(&count, 4, 1, f);
    for (int i = 0; i < chat_count; i++) {
        uint32_t role = chat_messages[i].role, len = strlen(chat_messages[i].text);
        fwrite(&role, 4, 1, f);
        fwrite(&len, 4, 1, f);
        fwrite(chat_messages[i].text, 1, len, f);
    }
    fclose(f);
    char name[48];snprintf(name,sizeof(name),"%016llx.chat",chat_id);
    lumen_storage_write(name,bytes,size);
    free(bytes);
}
static int chat_append(int role, const char *text) {
    if (chat_count == CHAT_LIMIT || !text || strlen(text) > 131072)
        return 0;
    char *copy = strdup(text);
    if (!copy)
        return 0;
    if (!chat_id) {
        struct timespec t;
        clock_gettime(CLOCK_REALTIME, &t);
        chat_id = (unsigned long long)t.tv_sec * 1000000 + (unsigned)t.tv_nsec / 1000;
    }
    chat_messages[chat_count].role = role;
    chat_messages[chat_count++].text = copy;
    if (chat_count == 1) {
        snprintf(chat_title, sizeof(chat_title), "%s", text);
        chat_title[strcspn(chat_title, "\r\n")] = 0;
    }
    chat_save();
    return 1;
}
static int chat_load(unsigned long long id) {
    char path[2048], magic[7];
    chat_path(path, sizeof(path), id);
    FILE *f = fopen(path, "rb");
    if(!f && legacy_root && *legacy_root) {
        snprintf(path,sizeof(path),"%s/chats/%016llx.chat",legacy_root,id);
        f=fopen(path,"rb");
    }
    if (!f)return 0;
    uint32_t count = 0;
    if (fread(magic, 1, 7, f) != 7 || memcmp(magic, "LCHAT1\n", 7) || fread(&count, 4, 1, f) != 1 ||
        count > CHAT_LIMIT) {
        fclose(f);
        return 0;
    }
    chat_clear();
    chat_id = id;
    for (unsigned i = 0; i < count; i++) {
        uint32_t role, len;
        if (fread(&role, 4, 1, f) != 1 || fread(&len, 4, 1, f) != 1 || role > 1 || len > 131072)
            break;
        char *s = calloc(1, len + 1);
        if (!s || fread(s, 1, len, f) != len) {
            free(s);
            break;
        }
        chat_messages[chat_count].role = role;
        chat_messages[chat_count++].text = s;
    }
    fclose(f);
    if (chat_count != (int)count) {
        chat_clear();
        return 0;
    }
    snprintf(chat_title, sizeof(chat_title), "%s", chat_count ? chat_messages[0].text : "");
    chat_title[strcspn(chat_title, "\r\n")] = 0;
    chat_page = 1000;
    llm_prompt16[0] = 0;
    return 1;
}
static void chat_scan_dir(const char *dir) {
    DIR *d = opendir(dir);
    if (!d)
        return;
    struct dirent *e;
    while ((e = readdir(d)) && chat_archive_count < 256) {
        unsigned long long id;
        int used = 0;
        if (sscanf(e->d_name, "%16llx.chat%n", &id, &used) != 1 || e->d_name[used])
            continue;
        int seen=0;
        for(int i=0;i<chat_archive_count;i++)if(chat_archive[i].id==id){seen=1;break;}
        if(seen)continue;
        char path[2048], magic[7];
        snprintf(path,sizeof(path),"%s/%016llx.chat",dir,id);
        FILE *f = fopen(path, "rb");
        if (!f)
            continue;
        uint32_t count, role, len;
        int ok = fread(magic, 1, 7, f) == 7 && !memcmp(magic, "LCHAT1\n", 7) &&
                 fread(&count, 4, 1, f) == 1 && count && fread(&role, 4, 1, f) == 1 &&
                 fread(&len, 4, 1, f) == 1;
        if (ok) {
            int at = chat_archive_count++;
            chat_archive[at].id = id;
            struct stat st={0};
            stat(path, &st);
            chat_archive[at].modified = st.st_mtime;
            size_t n = len < 99 ? len : 99;
            size_t got = fread(chat_archive[at].title, 1, n, f);
            chat_archive[at].title[got] = 0;
            chat_archive[at].title[strcspn(chat_archive[at].title, "\r\n")] = 0;
        }
        fclose(f);
    }
    closedir(d);
}
static void chat_scan(void) {
    chat_archive_count=0;
    chat_scan_dir(data_root);
    if(legacy_root && *legacy_root) {
        char dir[2048];snprintf(dir,sizeof(dir),"%s/chats",legacy_root);
        chat_scan_dir(dir);
    }
    for (int i = 0; i < chat_archive_count; i++)
        for (int j = i + 1; j < chat_archive_count; j++)
            if (chat_archive[j].modified > chat_archive[i].modified) {
                unsigned char tmp[sizeof(chat_archive[0])];
                memcpy(tmp, &chat_archive[i], sizeof(tmp));
                memcpy(&chat_archive[i], &chat_archive[j], sizeof(tmp));
                memcpy(&chat_archive[j], tmp, sizeof(tmp));
            }
}
static void chat_write_request(FILE *f, const char *model) {
    fputs("{\"model\":", f);
    chat_json(f,model);
    fputs(",\"input\":[", f);
    for (int i = 0; i < chat_count; i++) {
        if (i)
            fputc(',', f);
        fprintf(f, "{\"role\":\"%s\",\"content\":", chat_messages[i].role ? "assistant" : "user");
        chat_json(f,chat_messages[i].text);
        fputc('}', f);
    }
    fputs("],\"max_output_tokens\":4096,\"store\":false}", f);
}
