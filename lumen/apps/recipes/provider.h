#define LUMEN_JSON_IMPLEMENTATION
#include "lumen_json.h"
#include "lumen_http.h"
#include <ctype.h>
#include <strings.h>
#include <sys/wait.h>

static char *read_text(const char *root,const char *name,size_t limit) {
    char *text=malloc(limit+1);
    size_t size;
    if(!text)return NULL;
    if(!lumen_storage_read_at(root,name,text,limit,&size) || memchr(text,0,size)) {
        free(text);return NULL;
    }
    text[size]=0;
    return text;
}
static int recipe_download(const char *url,const char *name) {
    char *text=malloc(4194305);
    if(!text)return 0;
    int ok=lumen_https_get(url,text,4194305) && lumen_storage_write(name,text,strlen(text));
    free(text);
    return ok;
}
static void md_escape17(FILE *out,const char *text,size_t size) {
    for(size_t i=0;i<size;i++) {
        if(text[i]=='&')fputs("&amp;",out);
        else if(text[i]=='<')fputs("&lt;",out);
        else if(text[i]=='>')fputs("&gt;",out);
        else if(text[i]=='"')fputs("&quot;",out);
        else fputc((unsigned char)text[i],out);
    }
}
static int sanitize_fragment(FILE *output,const char *html,const char *title,char image[2048]) {
    const char *tool=getenv("LUMEN_DOCUMENT_TOOL");
    if(!tool || !*tool)return 0;
    FILE *input=tmpfile(),*result=tmpfile();
    if(!input || !result){if(input)fclose(input);if(result)fclose(result);return 0;}
    int ok=fputs(html,input)>=0 && !fflush(input) && !fseek(input,0,SEEK_SET);
    if(ok) {
        pid_t child=fork();
        if(!child) {
            if(dup2(fileno(input),STDIN_FILENO)<0 || dup2(fileno(result),STDOUT_FILENO)<0)_exit(1);
            execl(tool,tool,"--sanitize-html",title,(char *)NULL);
            _exit(1);
        }
        int status;
        pid_t done;
        do done=child>0 ? waitpid(child,&status,0) : -1; while(child>0 && done<0 && errno==EINTR);
        ok=child>0 && done==child && WIFEXITED(status) && WEXITSTATUS(status)==0 &&
           !fseek(result,0,SEEK_SET) && fgets(image,2048,result) && strchr(image,'\n');
        if(ok) {
            image[strcspn(image,"\n")]=0;
            char chunk[8192];size_t size;
            while((size=fread(chunk,1,sizeof(chunk),result)) && ok)
                ok=fwrite(chunk,1,size,output)==size;
            if(ferror(result))ok=0;
        }
    }
    fclose(result);fclose(input);
    return ok;
}
static void save_photo(const char *url,const char *path) {
    if((strncmp(url,"https://upload.wikimedia.org/",29) &&
        strncmp(url,"https://www.themealdb.com/",26)) || strpbrk(url,"\r\n\t "))return;
    FILE *image=tmpfile();
    if(!image)return;
    pid_t child=fork();
    if(!child) {
        if(dup2(fileno(image),STDOUT_FILENO)<0)_exit(1);
        execlp("curl","curl","-q","--fail","--silent","--proto","=https","--connect-timeout","4",
               "--max-time","8","--max-filesize","2097152","--",url,(char *)NULL);
        _exit(1);
    }
    if(child>0) {
        int status;pid_t done;
        do done=waitpid(child,&status,0);while(done<0 && errno==EINTR);
        if(done==child && WIFEXITED(status) && !WEXITSTATUS(status) && !fseek(image,0,SEEK_SET)) {
            unsigned char *bytes=malloc(2097153);
            if(bytes) {
                size_t size=fread(bytes,1,2097153,image);
                if(size && size<=2097152 && !ferror(image))lumen_storage_write(path,bytes,size);
                free(bytes);
            }
        }
    }
    fclose(image);
}
static void url_encode(const char *s, char *out, int cap) {
    int n = 0;
    for (; *s && n < cap - 4; s++) {
        unsigned char c = *s;
        if (isalnum(c) || strchr("-._~", c))
            out[n++] = c;
        else {
            snprintf(out + n, 4, "%%%02X", c);
            n += 3;
        }
    }
    out[n] = 0;
}
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
            snprintf(out,sizeof(out),"recipes-%s.json",langs[i]);
            lumen_storage_delete(out);
            int ok = recipe_download(url, out);
            _exit(ok ? 0 : 1);
        }
    }
    char encoded[7000], url[8192], out[4096];
    url_encode(q, encoded, sizeof(encoded));
    snprintf(url, sizeof(url), "https://www.themealdb.com/api/json/v1/1/search.php?s=%s", encoded);
    snprintf(out,sizeof(out),"meals.json");
    lumen_storage_delete(out);
    recipe_download(url, out);
    for (int i = 0; i < 3; i++)
        if (jobs[i] > 0)
            while (waitpid(jobs[i], NULL, 0) < 0 && errno == EINTR) {
            }
}
static void recipes_parse17(void) {
    recipes_count17 = 0;
    const char *langs[] = {"de", "en", "fr"};
    for (int pass = 0; pass < 4; pass++) {
        int language = pass < 3 ? (pass + (unsigned)lumen_language) % 3 : -1;
        char name[80];
        if (pass < 3)
            snprintf(name, 80, "recipes-%s.json", langs[language]);
        else
            snprintf(name, 80, "meals.json");
        char *s = read_text(getenv("LUMEN_APP_DATA"),name,4194304);
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
static int recipes_fetch17(const char *unused) {
    (void)unused;
    Recipe17 *r = &selected_recipe17;
    const char *out="recipe-detail.json";
    char url[4096];
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
    if (!recipe_download(url, out))
        return 0;
    char *s = read_text(getenv("LUMEN_APP_DATA"),out,4194304);
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
            md_escape17(f, word("Zutaten","Ingredients","Ingrédients"), strlen(word("Zutaten","Ingredients","Ingrédients")));
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
            md_escape17(f, word("Zubereitung","Directions","Préparation"), strlen(word("Zubereitung","Directions","Préparation")));
            fputs("</h2>", f);
            char instructions[30000];
            json_string(&j, json_get(&j, o, "strInstructions"), instructions, sizeof(instructions));
            fputs("<p style=\"white-space:pre-wrap\">",f);
            md_escape17(f,instructions,strlen(instructions));
            fputs("</p>",f);
            snprintf(source, sizeof(source), "https://www.themealdb.com/meal/%s", r->id);
            ok = *instructions != 0;
        }
    } else {
        int o = json_get(&j, 0, "parse"), v = json_get(&j, o, "text");
        char *html = calloc(1, 3000001);
        if (html) {
            json_string(&j, v, html, 3000001);
            ok=sanitize_fragment(f,html,r->title,image);
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
    char path[128],picture[128],*html=NULL;
    size_t html_size=0;
    snprintf(picture,sizeof(picture),"recipe-archive16/%s.jpg",r->id);
    save_photo(image,picture);
    int photo=lumen_storage_exists(picture);
    f=open_memstream(&html,&html_size);
    if(f) {
        fputs("<!doctype html><meta "
              "charset=utf-8><style>p,li{line-height:1.35}h1{font-size:1.7em;line-height:1.12}h2{"
              "margin-top:1em}img{width:100%;height:auto}table{border-collapse:collapse;width:100%}"
              "td,th{padding:.3em;border-bottom:1px solid #aaa}figure{margin:1em 0}</style><h1>",
              f);
        md_escape17(f, r->title, strlen(r->title));
        fputs("</h1>", f);
        if (photo)
            fprintf(f, "<figure><img src=\"%s.jpg\"></figure>", r->id);
        fwrite(body, 1, len, f);
        fprintf(f, "<p><a href=\"%s\">%s</a></p>", source,
                r->meal ? "TheMealDB" : "Wikibooks · CC BY-SA · source and contributors");
        ok=!fclose(f);
        snprintf(path,sizeof(path),"recipe-archive16/%s.html",r->id);
        if(ok)ok=lumen_storage_write(path,html,html_size);
        if(ok) {
            char metadata[2400];
            int length=snprintf(metadata,sizeof(metadata),"%s\n%s\n",r->title,source);
            snprintf(path,sizeof(path),"recipe-archive16/%s.txt",r->id);
            ok=lumen_storage_write(path,metadata,(size_t)length);
        }
    } else ok=0;
    free(html);free(body);
    return ok;
}
