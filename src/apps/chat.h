/* Local chat transcript and a bounded paper-like conversation viewport. */
#define CHAT_LIMIT 64
static struct {
    int role;
    char *text;
} chat_messages[CHAT_LIMIT];
static int chat_count, chat_ready, chat_page;
static unsigned long long chat_id;
static struct {
    unsigned long long id;
    time_t modified;
    char title[100];
} chat_archive[256];
static int chat_archive_count, chat_archive_page;
static struct {
    int message, y, height, truncated;
} chat_rows[CHAT_LIMIT];
static int chat_row_count, chat_pages;
static char chat_title[100];
static void chat_path(char *out, size_t size, unsigned long long id) {
    snprintf(out, size, "%s/chats/%016llx.chat", data_root, id);
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
    char dir[2048], path[2048];
    snprintf(dir, sizeof(dir), "%s/chats", data_root);
    mkdir(dir, 0700);
    chat_path(path, sizeof(path), chat_id);
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
    atomic_write(path, bytes, size);
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
    if (!f)
        return 0;
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
static void chat_scan(void) {
    char dir[2048];
    snprintf(dir, sizeof(dir), "%s/chats", data_root);
    DIR *d = opendir(dir);
    chat_archive_count = 0;
    if (!d)
        return;
    struct dirent *e;
    while ((e = readdir(d)) && chat_archive_count < 256) {
        unsigned long long id;
        int used = 0;
        if (sscanf(e->d_name, "%16llx.chat%n", &id, &used) != 1 || e->d_name[used])
            continue;
        char path[2048], magic[7];
        chat_path(path, sizeof(path), id);
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
            struct stat st;
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
    for (int i = 0; i < chat_archive_count; i++)
        for (int j = i + 1; j < chat_archive_count; j++)
            if (chat_archive[j].modified > chat_archive[i].modified) {
                unsigned char tmp[sizeof(chat_archive[0])];
                memcpy(tmp, &chat_archive[i], sizeof(tmp));
                memcpy(&chat_archive[i], &chat_archive[j], sizeof(tmp));
                memcpy(&chat_archive[j], tmp, sizeof(tmp));
            }
}
static void chat_write_request(FILE *f, const char *model, const char *prompt) {
    (void)prompt;
    fputs("{\"model\":", f);
    json_escape16(f, model);
    fputs(",\"input\":[", f);
    for (int i = 0; i < chat_count; i++) {
        if (i)
            fputc(',', f);
        fprintf(f, "{\"role\":\"%s\",\"content\":", chat_messages[i].role ? "assistant" : "user");
        json_escape16(f, chat_messages[i].text);
        fputc('}', f);
    }
    fputs("],\"max_output_tokens\":4096,\"store\":false}", f);
}
static void chat_complete(const char *text) {
    chat_append(1, text);
    chat_page = 1000;
    llm_prompt16[0] = 0;
    extra_write_text("llm-prompt16.txt", "");
}
static void chat_preview(int i, char *out, size_t size) {
    const char *p = chat_messages[i].text;
    size_t n = 0;
    while (*p && n + 1 < size) {
        if ((p[0] == '*' && p[1] == '*') || (p[0] == '`' && p[1] == '`')) {
            p += 2;
            continue;
        }
        if ((*p == '#' && (p == chat_messages[i].text || p[-1] == '\n')) || *p == '`') {
            p++;
            continue;
        }
        out[n++] = *p++;
    }
    out[n] = 0;
}
static void chat_layout(void) {
    int y = 174, page_index = 0;
    chat_row_count = 0;
    for (int i = 0; i < chat_count; i++) {
        char b[2048];
        chat_preview(i, b, sizeof(b));
        FlowLine lines[6];
        int limit = chat_messages[i].role ? 5 : 3, n = text_flow(b, &small, 500, lines, limit);
        int truncated =
            (size_t)lines[n - 1].end < strlen(b) || strlen(chat_messages[i].text) >= sizeof(b);
        int height = 27 + n * 26 + (truncated ? 44 : 12);
        if (y + height > 615 && y > 174) {
            page_index++;
            y = 174;
        }
        if (page_index == chat_page)
            chat_rows[chat_row_count++] = (typeof(chat_rows[0])){i, y, height, truncated};
        y += height + 16;
    }
    chat_pages = page_index + 1;
    if (chat_page >= chat_pages) {
        chat_page = chat_pages - 1;
        chat_layout();
    }
}
static void chat_draw(void) {
    if (!chat_ready) {
        chat_ready = 1;
        config16("llm-model.txt", llm_model16, sizeof(llm_model16));
        config16("llm-prompt16.txt", llm_prompt16, sizeof(llm_prompt16));
    }
    heading_actions("OpenAI", 3);
    icon_button(408, 74, IC_PLUS);
    icon_button(468, 74, IC_CONTENTS);
    icon_button(528, 74, IC_SETTINGS);
    ink_color = 125;
    drawtext_raw(30, 139, chat_count ? llm_model16 : tr("Ein Gedanke. Ein Gespräch."), &caption,
                 540);
    ink_color = 0;
    chat_layout();
    if (!chat_count) {
        icon(278, 280, IC_TYPE);
        centered(28, 350, 544, 48, "Was möchtest du verstehen?", &editorial);
        wrapped(52, 410, "Stelle eine Frage. Den nächsten Gedanken kannst du direkt anschließen.",
                &small, 496, 3);
    }
    for (int row = 0; row < chat_row_count; row++) {
        int i = chat_rows[row].message, y = chat_rows[row].y, bot = chat_messages[i].role;
        char b[2048];
        chat_preview(i, b, sizeof(b));
        ink_color = 125;
        drawtext(40, y, bot ? "OpenAI" : "Du", &caption, 510);
        ink_color = 0;
        text_flow_draw(40, y + 26, 500, b, &small, 26, bot ? 5 : 3, 0);
        if (chat_rows[row].truncated) {
            int yy = y + chat_rows[row].height - 40;
            drawtext_raw(40, yy - 15, "…", &small, 35);
            button_h(76, yy, 280, 36, "Weiterlesen", 0);
        }
        divider(28, y + chat_rows[row].height + 5, 544);
    }
    key_outline(28, 634, 470, 91);
    region(28, 634, 470, 91);
    ink_color = *llm_prompt16 ? 0 : 125;
    wrapped_raw(42, 646, *llm_prompt16 ? llm_prompt16 : tr("Nachricht schreiben …"), &small, 442,
                2);
    ink_color = 0;
    icon_button(528, 655, IC_NEXT);
    pager(746, &chat_page, chat_pages);
    if (job_pid16 > 0 && job_kind16 == 2) {
        rect(28, 593, 544, 32, 255);
        drawtext(40, 595, "Antwort wird geschrieben …", &caption, 530);
    }
}
static int chat_input(const Input *in) {
    if (in->kind != INPUT_TAP)
        return 2;
    int x = in->x, y = in->y;
    if (y < 48)
        return -1;
    if (y >= 74 && y < 118) {
        if (x >= 516)
            extra_open(123);
        else if (job_pid16 > 0)
            ui_message("Bitte die laufende Anfrage abwarten.");
        else if (x >= 456) {
            chat_scan();
            chat_archive_page = 0;
            extra_open(128);
        } else if (x >= 396) {
            chat_clear();
            chat_title[0] = llm_prompt16[0] = 0;
        }
        return 1;
    }
    if (y >= 634 && y < 728) {
        if (x < 510)
            open_keyboard(174, "Nachricht", llm_prompt16);
        else if (job_pid16 > 0)
            ui_message("Bitte die laufende Anfrage abwarten.");
        else if (*llm_prompt16) {
            char key[2049];
            config16("llm-key.txt", key, sizeof(key));
            if (!*key) {
                ui_message("Bitte zuerst einen API-Schlüssel einrichten.");
                return 1;
            }
            if ((chat_count && chat_messages[chat_count - 1].role == 0 &&
                 !strcmp(chat_messages[chat_count - 1].text, llm_prompt16)) ||
                chat_append(0, llm_prompt16)) {
                chat_page = 1000;
                job_start16(2, llm_prompt16);
            } else
                ui_message("Chat ist voll. Bitte einen neuen Chat beginnen.");
        }
        return 1;
    }
    for (int r = 0; r < chat_row_count; r++)
        if (chat_rows[r].truncated && y >= chat_rows[r].y &&
            y < chat_rows[r].y + chat_rows[r].height) {
            int i = chat_rows[r].message;
            reader_markdown17(chat_title, chat_messages[i].text, 122);
            return 1;
        }
    return 1;
}
static void chats_draw(void) {
    heading_actions("Chats", 1);
    icon_button(528, 74, IC_PLUS);
    for (int r = 0; r < 7 && chat_archive_page * 7 + r < chat_archive_count; r++) {
        int i = chat_archive_page * 7 + r, y = 148 + r * 76;
        char date[50];
        strftime(date, sizeof(date), "%d.%m.%Y · %H:%M", localtime(&chat_archive[i].modified));
        list_row(28, y, 544, 76, IC_TYPE, 0, chat_archive[i].title, date, &small, 1);
    }
    if (!chat_archive_count)
        wrapped(32, 240, "Deine Gespräche werden hier auf diesem Kindle gespeichert.", &editorial,
                530, 4);
    pager(746, &chat_archive_page, (chat_archive_count + 6) / 7);
}
static int chats_input(const Input *in) {
    if (in->kind != INPUT_TAP)
        return 2;
    if (in->y < 48)
        return -1;
    if (in->y >= 74 && in->y < 118 && in->x >= 516) {
        chat_clear();
        chat_title[0] = llm_prompt16[0] = 0;
        extra_return(122);
        return 1;
    }
    if (in->y >= 148 && in->y < 680) {
        int i = chat_archive_page * 7 + (in->y - 148) / 76;
        if (i < chat_archive_count && chat_load(chat_archive[i].id))
            extra_return(122);
    }
    return 1;
}
