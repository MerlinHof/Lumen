#include <mupdf/fitz.h>
typedef struct {
    char title[256], id[64], lang[4];
    int meal;
} Recipe17;
static Recipe17 recipes17[128], selected_recipe17;
static int recipes_count17;
static void recipes_search17(const char *q) {
    const char *langs[] = {"de", "en", "fr"};
    pid_t jobs[3] = {0};
    for (int i = 0; i < 3; i++) {
        jobs[i] = fork();
        if (!jobs[i]) {
#ifndef LUMEN_HOST
            prctl(PR_SET_PDEATHSIG, SIGTERM);
            if (getppid() == 1)
                _exit(1);
#endif
            char term[2048], encoded[7000], url[8192], out[4096];
            snprintf(term, sizeof(term),
                     i == 0   ? "intitle:Kochbuch %s -intitle:Techniken -intitle:Kochkurs "
                                "-intitle:Projektideen"
                     : i == 1 ? "%s"
                              : "intitle:Cuisine %s",
                     q);
            url_encode(term, encoded, sizeof(encoded));
            snprintf(url, sizeof(url),
                     "https://%s.wikibooks.org/w/"
                     "api.php?action=query&list=search&srsearch=%s&srnamespace=%s&srlimit=25&"
                     "format=json&formatversion=2",
                     langs[i], encoded, i == 1 ? "102" : "0");
            snprintf(out, sizeof(out), "%s/recipes-%s17.json", data_root, langs[i]);
            unlink(out);
            int ok = curl16(url, out, NULL, NULL);
            _exit(ok ? 0 : 1);
        }
    }
    char encoded[7000], url[8192], out[4096];
    url_encode(q, encoded, sizeof(encoded));
    snprintf(url, sizeof(url), "https://www.themealdb.com/api/json/v1/1/search.php?s=%s", encoded);
    local_path(out, sizeof(out), "recipes16.json");
    unlink(out);
    curl16(url, out, NULL, NULL);
    for (int i = 0; i < 3; i++)
        if (jobs[i] > 0)
            while (waitpid(jobs[i], NULL, 0) < 0 && errno == EINTR) {
            }
}
static void recipes_parse17(void) {
    recipes_count17 = 0;
    const char *langs[] = {"de", "en", "fr"};
    for (int pass = 0; pass < 4; pass++) {
        int language = pass < 3 ? (pass + options[OPT_LANGUAGE]) % 3 : -1;
        char name[80];
        if (pass < 3)
            snprintf(name, 80, "recipes-%s17.json", langs[language]);
        else
            snprintf(name, 80, "recipes16.json");
        char *s = service_read(name, 4194304);
        if (!s)
            continue;
        Json j;
        JsonToken *t = calloc(60000, sizeof(*t));
        if (t && json_parse(&j, s, t, 60000)) {
            int array = pass < 3 ? json_get(&j, json_get(&j, 0, "query"), "search")
                                 : json_get(&j, 0, "meals");
            for (int n = 0; n < 64 && recipes_count17 < 128; n++) {
                int o = json_at(&j, array, n);
                if (o < 0)
                    break;
                Recipe17 *r = &recipes17[recipes_count17];
                memset(r, 0, sizeof(*r));
                json_string(&j, json_get(&j, o, pass < 3 ? "title" : "strMeal"), r->title, 256);
                const char *prefixes[] = {"Kochbuch/", "Cookbook:", "Livre de cuisine/", NULL};
                for (int k = 0; prefixes[k]; k++) {
                    size_t len = strlen(prefixes[k]);
                    if (!strncasecmp(r->title, prefixes[k], len)) {
                        const char *start = r->title + len;
                        while (*start == ' ')
                            start++;
                        memmove(r->title, start, strlen(start) + 1);
                        break;
                    }
                }
                r->meal = pass == 3;
                if (pass < 3) {
                    snprintf(r->lang, 4, "%s", langs[language]);
                    int id = json_number(&j, json_get(&j, o, "pageid"));
                    if (id <= 0)
                        continue;
                    snprintf(r->id, 64, "wiki-%s-%d", r->lang, id);
                } else
                    json_string(&j, json_get(&j, o, "idMeal"), r->id, 64);
                int valid = *r->id != 0;
                for (const char *p = r->id; *p; p++)
                    if (!isalnum((unsigned char)*p) && *p != '-')
                        valid = 0;
                if (*r->title && valid)
                    recipes_count17++;
            }
        }
        free(t);
        free(s);
    }
}
static const char *html_tag17(fz_xml *x) {
    const char *s = fz_xml_tag(x);
    return s ? s : "";
}
static void recipe_text17(FILE *f, const char *text) {
    const char *p = text;
    while (*p) {
        const char *start = p;
        unsigned code = glyph(&p);
        if ((code >= 0x2300 && code <= 0x27ff) || (code >= 0xe000 && code <= 0xf8ff) ||
            (code >= 0x1f000 && code <= 0x1ffff))
            fputs(" · ", f);
        else
            md_escape17(f, start, p - start);
    }
}
static void recipe_flat17(fz_xml *x, char *out, int cap, int depth) {
    if (depth > 25)
        return;
    for (; x; x = fz_xml_next(x)) {
        const char *t = fz_xml_text(x);
        if (t) {
            int n = strlen(out);
            snprintf(out + n, cap - n, "%s", t);
        } else
            recipe_flat17(fz_xml_down(x), out, cap, depth + 1);
    }
}
static void recipe_html17(FILE *f, fz_xml *x, int depth, char *image, int cap) {
    if (depth > 40)
        return;
    for (; x; x = fz_xml_next(x)) {
        const char *text = fz_xml_text(x);
        if (text) {
            recipe_text17(f, text);
            continue;
        }
        const char *tag = html_tag17(x), *class = fz_xml_att(x, "class");
        if (!strcmp(tag, "h1") || !strcmp(tag, "h2")) {
            char heading[512] = "";
            recipe_flat17(fz_xml_down(x), heading, sizeof(heading), 0);
            size_t len = strlen(heading);
            while (len && isspace((unsigned char)heading[len - 1]))
                heading[--len] = 0;
            const char *start = heading;
            while (*start && isspace((unsigned char)*start))
                start++;
            if (!strcasecmp(start, selected_recipe17.title))
                continue;
        }

        if (!strcmp(tag, "script") || !strcmp(tag, "style") || !strcmp(tag, "form") ||
            !strcmp(tag, "nav") ||
            (class &&
             (strstr(class, "mw-editsection") || strstr(class, "navbox") ||
              strstr(class, "metadata") || strstr(class, "toc") || strstr(class, "noprint"))))
            continue;
        if (!strcmp(tag, "img") && !*image) {
            const char *src = fz_xml_att(x, "src");
            if (src && (!strncmp(src, "//upload.wikimedia.org/", 23) ||
                        !strncmp(src, "https://upload.wikimedia.org/", 29)))
                snprintf(image, cap, "%s%s", src[0] == '/' ? "https:" : "", src);
        }
        const char *allowed[] = {"p",  "h1",    "h2",    "h3",         "h4",  "ul",   "ol",
                                 "li", "table", "thead", "tbody",      "tr",  "th",   "td",
                                 "dl", "dt",    "dd",    "blockquote", "pre", "code", "strong",
                                 "b",  "em",    "i",     "br",         NULL};
        int keep = 0;
        for (int i = 0; allowed[i]; i++)
            if (!strcmp(tag, allowed[i]))
                keep = 1;
        if (keep)
            fprintf(f, "<%s>", tag);
        recipe_html17(f, fz_xml_down(x), depth + 1, image, cap);
        if (keep && strcmp(tag, "br"))
            fprintf(f, "</%s>", tag);
    }
}
static int recipes_fetch17(const char *unused) {
    (void)unused;
    Recipe17 *r = &selected_recipe17;
    char out[4096], url[4096];
    local_path(out, sizeof(out), "recipe-detail17.json");
    if (r->meal)
        snprintf(url, sizeof(url), "https://www.themealdb.com/api/json/v1/1/lookup.php?i=%s",
                 r->id);
    else {
        const char *id = strrchr(r->id, '-');
        if (!id)
            return 0;
        snprintf(url, sizeof(url),
                 "https://%s.wikibooks.org/w/"
                 "api.php?action=parse&pageid=%s&prop=text&format=json&formatversion=2",
                 r->lang, id + 1);
    }
    if (!curl16(url, out, NULL, NULL))
        return 0;
    char *s = service_read("recipe-detail17.json", 4194304);
    if (!s)
        return 0;
    Json j;
    JsonToken *t = calloc(90000, sizeof(*t));
    int ok = 0;
    if (!t || !json_parse(&j, s, t, 90000)) {
        free(t);
        free(s);
        return 0;
    }
    char *body = NULL;
    size_t len = 0;
    FILE *f = open_memstream(&body, &len);
    char image[2048] = "", source[2048] = "";
    if (!f) {
        free(t);
        free(s);
        return 0;
    }
    if (r->meal) {
        int o = json_at(&j, json_get(&j, 0, "meals"), 0);
        if (o >= 0) {
            json_string(&j, json_get(&j, o, "strMealThumb"), image, sizeof(image));
            fputs("<h2>", f);
            md_escape17(f, tr("Zutaten"), strlen(tr("Zutaten")));
            fputs("</h2><ul>", f);
            for (int i = 1; i <= 20; i++) {
                char key[40], ingredient[256], measure[256];
                snprintf(key, 40, "strIngredient%d", i);
                json_string(&j, json_get(&j, o, key), ingredient, 256);
                snprintf(key, 40, "strMeasure%d", i);
                json_string(&j, json_get(&j, o, key), measure, 256);
                if (*ingredient) {
                    fputs("<li>", f);
                    md_escape17(f, measure, strlen(measure));
                    fputc(' ', f);
                    md_escape17(f, ingredient, strlen(ingredient));
                    fputs("</li>", f);
                }
            }
            fputs("</ul><h2>", f);
            md_escape17(f, tr("Zubereitung"), strlen(tr("Zubereitung")));
            fputs("</h2>", f);
            char instructions[30000];
            json_string(&j, json_get(&j, o, "strInstructions"), instructions, sizeof(instructions));
            markdown_render17(f, instructions);
            snprintf(source, sizeof(source), "https://www.themealdb.com/meal/%s", r->id);
            ok = *instructions != 0;
        }
    } else {
        int o = json_get(&j, 0, "parse"), v = json_get(&j, o, "text");
        char *html = calloc(1, 3000001);
        if (html) {
            json_string(&j, v, html, 3000001);
            fz_context *c = fz_new_context(NULL, NULL, 4 * 1024 * 1024);
            fz_xml *x = NULL;
            fz_buffer *b = NULL;
            fz_var(x);
            fz_var(b);
            fz_try(c) {
                b = fz_new_buffer_from_copied_data(c, (unsigned char *)html, strlen(html));
                x = fz_parse_xml_from_html5(c, b);
                recipe_html17(f, fz_xml_root(x), 0, image, sizeof(image));
                ok = 1;
            }
            fz_always(c) {
                fz_drop_xml(c, x);
                fz_drop_buffer(c, b);
            }
            fz_catch(c) {
                ok = 0;
            }
            fz_drop_context(c);
            free(html);
        }
        snprintf(source, sizeof(source), "https://%s.wikibooks.org/?curid=%s", r->lang,
                 strrchr(r->id, '-') + 1);
    }
    fclose(f);
    free(t);
    free(s);
    if (!ok) {
        free(body);
        return 0;
    }
    char dir[4096], path[4096], raw[4096], pgm[4096];
    local_path(dir, sizeof(dir), "recipe-archive16");
    mkdir(dir, 0700);
    snprintf(path, sizeof(path), "%s/%s.html", dir, r->id);
    snprintf(raw, sizeof(raw), "%s/%s.jpg", dir, r->id);
    snprintf(pgm, sizeof(pgm), "%s/%s.pgm", dir, r->id);
    int photo = 0;
    if (*image && !strncmp(image, "https://", 8) && curl16(image, raw, NULL, NULL))
        photo = engine_image_fit(raw, pgm, 520, 340);
    unlink(raw);
    f = fopen(path, "w");
    if (f) {
        fputs("<!doctype html><meta "
              "charset=utf-8><style>p,li{line-height:1.35}h1{font-size:1.7em;line-height:1.12}h2{"
              "margin-top:1em}img{width:100%;height:auto}table{border-collapse:collapse;width:100%}"
              "td,th{padding:.3em;border-bottom:1px solid #aaa}figure{margin:1em 0}</style><h1>",
              f);
        md_escape17(f, r->title, strlen(r->title));
        fputs("</h1>", f);
        if (photo)
            fprintf(f, "<figure><img src=\"%s.pgm\"></figure>", r->id);
        fwrite(body, 1, len, f);
        fprintf(f, "<p><a href=\"%s\">%s</a></p>", source,
                r->meal ? "TheMealDB" : "Wikibooks · CC BY-SA · source and contributors");
        fclose(f);
        snprintf(path, sizeof(path), "%s/%s.txt", dir, r->id);
        f = fopen(path, "w");
        if (f) {
            fprintf(f, "%s\n%s", r->title, source);
            fclose(f);
        }
    }
    free(body);
    return f != NULL;
}
static void recipes_open_ready17(void) {
    recipe_archive16();
    if (screen != 114)
        return;
    char path[4096];
    snprintf(path, sizeof(path), "%s/recipe-archive16/%s.html", data_root, selected_recipe17.id);
    if (file_open16(path, 114))
        snprintf(library[BOOK_LIMIT].title, sizeof(library[BOOK_LIMIT].title), "%s",
                 selected_recipe17.title);
}
static int recipes_draw17(void) {
    if (screen != 114)
        return 0;
    heading_actions("Rezepte", 2);
    search_action();
    icon_button(468, 74, IC_HISTORY);
    drawtext(30, 140, recipe_search16 ? "Suchergebnisse" : "Zuletzt geöffnet · offline verfügbar",
             ui_font(UI_SECONDARY), 540);
    int count = recipe_search16 ? recipes_count17 : recipe_recent_count16;
    for (int row = 0; row < 6 && companion_page16 * 6 + row < count; row++) {
        int i = companion_page16 * 6 + row, y = 189 + row * 80;
        char source[100];
        if (recipe_search16)
            snprintf(source, sizeof(source), recipes17[i].meal ? "TheMealDB" : "Wikibooks · %s",
                     recipes17[i].lang);
        else
            snprintf(source, sizeof(source), "%s", tr("Offline verfügbar"));
        list_row(28, y, 544, 80, IC_BOOK, 0,
                 recipe_search16 ? recipes17[i].title : recipe_recent16[i].title, source, ui_font(UI_BODY),
                 2);
    }
    pager(715, &companion_page16, (count + 5) / 6);
    if (!count)
        wrapped(30, 260, "Über die Lupe nach einem Gericht suchen.", ui_font(UI_PRIMARY), 540, 3);
    if (job_pid16 > 0 && (job_kind16 == 1 || job_kind16 == 4)) {
        rect(60, 292, 480, 146, 255);
        key_outline(60, 292, 480, 146);
        centered(70, 315, 460, 100, "Rezepte werden geladen …", ui_font(UI_PRIMARY));
    }
    return 1;
}
static int recipes_input17(const Input *in) {
    if (screen != 114 || in->kind != INPUT_TAP)
        return -1;
    int x = in->x, y = in->y;
    if (search_action_hit(x, y)) {
        open_keyboard(172, "Rezepte suchen", "");
        return 1;
    }
    if (y >= 74 && y < 118 && x >= 460 && x < 516) {
        recipe_search16 = 0;
        recipe_archive16();
        companion_page16 = 0;
        return 1;
    }
    if (y >= 189 && y < 669) {
        int i = companion_page16 * 6 + (y - 189) / 80;
        if (recipe_search16) {
            if (i < recipes_count17) {
                selected_recipe17 = recipes17[i];
                job_start16(4, "");
            }
        } else if (i < recipe_recent_count16) {
            char path[4096];
            snprintf(path, sizeof(path), "%s/recipe-archive16/%s.html", data_root,
                     recipe_recent16[i].id);
            if (!access(path, R_OK)) {
                if (file_open16(path, 114))
                    snprintf(library[BOOK_LIMIT].title, sizeof(library[BOOK_LIMIT].title), "%s",
                             recipe_recent16[i].title);
            } else {
                char n[160];
                snprintf(n, sizeof(n), "recipe-archive16/%s.txt", recipe_recent16[i].id);
                char *s = service_read(n, 65536);
                if (s) {
                    char *p = strchr(s, '\n');
                    reader16(recipe_recent16[i].title, p ? p + 1 : s, 114);
                    free(s);
                }
            }
        }
        return 1;
    }
    return -1;
}

static int recipes_back(void) {
    if (screen != 114 || !recipe_search16)
        return 0;
    recipe_search16 = 0;
    companion_page16 = 0;
    recipe_archive16();
    return 1;
}
