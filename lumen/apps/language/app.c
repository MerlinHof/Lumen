#include "lumen_app.h"
#include "lumen_fsrs.h"
#include "lumen_text.h"
#include "lessons.h"
#include "strings.h"

static int lang_pair,lang_index,lang_reveal,glossary;
static const char *tr(const char *key) {
    int language=lumen_language>=0 && lumen_language<3 ? lumen_language : 0;
    for(size_t i=0;i<sizeof(strings)/sizeof(*strings);i++)
        if(!strcmp(strings[i].key,key))return strings[i].values[language];
    return key;
}
static int today_key(void) {
    time_t now=time(NULL);
    struct tm date;
    localtime_r(&now,&date);
    return (date.tm_year+1900)*10000+(date.tm_mon+1)*100+date.tm_mday;
}

#define UI_PRIMARY PAINT_HEADING
#define UI_SECONDARY PAINT_SECONDARY
#define UI_DISPLAY PAINT_DISPLAY
#define UI_TITLE PAINT_TITLE
#define UI_BODY PAINT_BODY
#define ui_font(r) (r)
#define heading_actions ui_paint_heading
#define icon_button ui_paint_icon_button
#define rect(x,y,w,h,c) ui_paint_surface(x,y,w,h,0,c,c)
#define label_at(x,y,w,h,s,r) lumen_text(x,y,w,h,tr(s),r,-1,1)
#define centered(x,y,w,h,s,r) ui_paint_text(x,y,w,h,tr(s),r)
#define drawtext_raw(x,y,s,r,w) label_at(x,y,w,34,s,r)
#define wrapped_raw(x,y,s,r,w,n) lumen_wrapped(x,y,w,40*(n),tr(s),r,n)
#define divider(x,y,w) ui_paint_line(x,y,x+w,y,1,190)

static struct {
    MemoryCard cards[3][24];
    int attempts, correct, day, count, lastday, streak;
} language_progress;
static int language_ready, language_tab, language_answered, language_options[4],
    language_choice = -1, language_book_page;
static char language_written[512];
static int language_source(void) {
    return lang_pair == 2 ? 1 : 0;
}
static int language_target(void) {
    return lang_pair == 0 ? 1 : lang_pair == 1 ? 2 : 0;
}
static void language_init(void) {
    if (language_ready)
        return;
    language_ready = 1;
    if(!app_state_load("progress.bin",&language_progress,sizeof(language_progress))) {
        unsigned char legacy[8+sizeof(language_progress)];
        if(lumen_legacy_state("language-plus-1.1.bin",legacy,sizeof(legacy))) {
            uint32_t header[2];memcpy(header,legacy,8);
            if(header[0]==0x4c554d16 && header[1]==sizeof(language_progress))
                memcpy(&language_progress,legacy+8,sizeof(language_progress));
        }
    }
    int saved;
    if(app_state_load("pair.bin",&saved,sizeof(saved))) {
        if(saved>=0 && saved<3)lang_pair=saved;
    } else {
        uint32_t old[7];
        if(lumen_legacy_state("language-1.bin",old,sizeof(old)) &&
           old[0]==0x4c554d16 && old[1]==20 && old[2]<3)lang_pair=(int)old[2];
    }
}
static void language_new(void) {
    language_init();
    language_answered = 0;
    language_choice = -1;
    lang_reveal = 0;
    language_written[0] = 0;
    int found = -1;
    for (int i = 0; i < 24; i++)
        if (language_progress.cards[lang_pair][i].due <= time(NULL) &&
            (found < 0 || language_progress.cards[lang_pair][i].due <
                              language_progress.cards[lang_pair][found].due))
            found = i;
    lang_index = found >= 0 ? found : rand() % 24;
    int correct = rand() % 4;
    for (int i = 0; i < 4; i++) {
        if (i == correct) {
            language_options[i] = lang_index;
            continue;
        }
        int q;
        do {
            q = rand() % 24;
            int dup = q == lang_index;
            for (int j = 0; j < i; j++)
                if (language_options[j] == q)
                    dup = 1;
            if (!dup)
                break;
        } while (1);
        language_options[i] = q;
    }
}
static void language_grade(int good) {
    language_progress.attempts++;
    language_progress.correct += good;
    int today = today_key();
    if (language_progress.day != today) {
        language_progress.day = today;
        language_progress.count = 0;
    }
    language_progress.count++;
    if (language_progress.lastday != today) {
        time_t t = time(NULL) - 86400;
        struct tm y;
        localtime_r(&t, &y);
        int yesterday = (y.tm_year + 1900) * 10000 + (y.tm_mon + 1) * 100 + y.tm_mday;
        language_progress.streak =
            language_progress.lastday == yesterday ? language_progress.streak + 1 : 1;
        language_progress.lastday = today;
    }
    fsrs_review(&language_progress.cards[lang_pair][lang_index], good ? 3 : 1, time(NULL));
    app_state_save("progress.bin", &language_progress, sizeof(language_progress));
    language_answered = good ? 1 : 2;
}
static void fold_answer(const char *input,char *out,size_t capacity) {
    lumen_utf8_fold(input,out,capacity);
    char *write=out;
    for(const unsigned char *read=(const unsigned char *)out;*read;read++)
        if(isalnum(*read))*write++=(char)*read;
    *write=0;
}
static void language_check_written(const char *text) {
    snprintf(language_written,sizeof(language_written),"%.*s",(int)sizeof(language_written)-1,text);
    if(!*language_written)return;
    char expected[512],actual[512];
    fold_answer(v1_lessons[lang_index][language_target()],expected,sizeof(expected));
    fold_answer(language_written,actual,sizeof(actual));
    language_grade(!strcmp(expected,actual));
}
static void language_book(void);
static void draw(void) {
    language_init();
    static int started;
    if (!started) {
        started = 1;
        language_new();
    }
    if(glossary){language_book();return;}
    heading_actions(tr("Sprachen"), 1);
    lumen_icon_action("glossary",528,74,IC_BOOK);
    const char *pair[] = {"DE → EN", "DE → FR", "EN → DE"};
    lumen_segments("pair",28,132,544,39,pair,3,lang_pair);
    const char *tabs[] = {tr("Karten"),tr("Quiz"),tr("Schreiben"),tr("Fortschritt")};
    lumen_segments("practice",28,184,544,42,tabs,4,language_tab);
    char b[180];
    if (language_tab == 3) {
        int known = 0, due = 0;
        for (int i = 0; i < 24; i++) {
            MemoryCard *c = &language_progress.cards[lang_pair][i];
            known += c->reviews > 0;
            due += c->due <= time(NULL);
        }
        snprintf(b, 180, "%d / 24", known);
        centered(28, 272, 544, 70, b, ui_font(UI_DISPLAY));
        centered(28, 347, 544, 40, "Ausdrücke geübt", ui_font(UI_SECONDARY));
        snprintf(b, 180, "%d %s · %d %s", due, tr("Fällig"), language_progress.streak,
                 tr("Tage in Folge"));
        centered(28, 413, 544, 42, b, ui_font(UI_PRIMARY));
        snprintf(b, 180, "%d / %d", language_progress.correct, language_progress.attempts);
        centered(28, 493, 544, 42, b, ui_font(UI_PRIMARY));
        centered(28, 538, 544, 32, "Richtige Antworten", ui_font(UI_SECONDARY));
        lumen_button_action("due",28, 620, 544, 60, tr("Fällige Ausdrücke üben"), 0);
        return;
    }
    wrapped_raw(30, 258, v1_lessons[lang_index][language_source()], ui_font(UI_TITLE), 540, 3);
    if (language_tab == 0) {
        if (!lang_reveal) {
            lumen_button_action("reveal",28, 470, 544, 60, tr("Antwort zeigen"), 0);
        } else {
            wrapped_raw(30, 389, v1_lessons[lang_index][language_target()], ui_font(UI_PRIMARY), 540, 3);
            lumen_button_action("again",28, 548, 260, 58, tr("Noch üben"), 0);
            lumen_button_action("known",312, 548, 260, 58, tr("Gewusst"), 0);
        }
    } else if (language_tab == 1) {
        for (int i = 0; i < 4; i++) {
            int y = 352 + i * 72;
            char id[12];snprintf(id,sizeof(id),"answer-%d",i);
            lumen_button_action(id,28,y,544,60,v1_lessons[language_options[i]][language_target()],
                     language_answered && language_options[i] == lang_index);
        }
        if (language_answered) {
            label_at(30, 642, 540, 35,
                     tr(language_answered == 1 ? "Richtig!" : "Die richtige Antwort ist markiert."),
                     ui_font(UI_SECONDARY));
            lumen_button_action("next",28, 681, 544, 46, tr("Weiter"), 0);
        }
    } else {
        lumen_button_action("write",28,391,544,62,
                 *language_written ? language_written : tr("Übersetzung schreiben"), 0);
        if (language_answered) {
            wrapped_raw(30, 492, v1_lessons[lang_index][language_target()], ui_font(UI_PRIMARY), 540, 3);
            label_at(30, 590, 540, 35,
                     tr(language_answered == 1 ? "Richtig!"
                                               : "Vergleiche deine Antwort mit der Lösung."),
                     ui_font(UI_SECONDARY));
            lumen_button_action("next",28, 658, 544, 60, tr("Weiter"), 0);
        }
    }
    if (language_tab == 0 || !language_answered) {
        int count = language_progress.day == today_key() ? language_progress.count : 0;
        snprintf(b, 180, "%d / 10 · %s", count,
                 tr(count >= 10 ? "Tagesziel erreicht!" : "Tagesziel"));
        label_at(30, 653, 540, 32, b, ui_font(UI_SECONDARY));
        rect(30, 701, 540, 5, 215);
        rect(30, 701, 54 * (count > 10 ? 10 : count), 5, 0);
    }
}
static int input(const Input *in) {
    if(in->kind==INPUT_BACK){glossary=lumen_back=0;return 1;}
    if(in->kind==INPUT_PAGE && glossary){language_book_page=in->x;return 1;}
    if(in->kind==INPUT_TEXT && in->x==1 && !glossary) {
        char text[512];
        if(lumen_string("text",text,sizeof(text)))language_check_written(text);
        return 1;
    }
    if(in->kind!=INPUT_COMPONENT || glossary)return 0;
    char id[32],gesture[24];
    if(!lumen_string("id",id,sizeof(id)) || !lumen_string("gesture",gesture,sizeof(gesture)))return 0;
    if(!strcmp(gesture,"selected")) {
        char value[8];
        if(!lumen_string("text",value,sizeof(value)) || strlen(value)!=1 || value[0]<'0' || value[0]>'3')return 0;
        int choice=value[0]-'0';
        if(!strcmp(id,"pair") && choice<3) {
            lang_pair=choice;app_state_save("pair.bin",&lang_pair,sizeof(lang_pair));language_new();
        } else if(!strcmp(id,"practice")){language_tab=choice;language_new();}
    } else if(!strcmp(gesture,"clicked")) {
        if(!strcmp(id,"glossary")){glossary=lumen_back=1;language_book_page=0;}
        else if(!strcmp(id,"due")){language_tab=0;language_new();}
        else if(!strcmp(id,"reveal"))lang_reveal=1;
        else if(!strcmp(id,"again") || !strcmp(id,"known")) {
            language_grade(!strcmp(id,"known"));language_new();
        } else if(!strcmp(id,"next"))language_new();
        else if(!strcmp(id,"write") && !language_answered)
            lumen_request("input",tr("Übersetzung"),language_written,1);
        else if(!language_answered && strlen(id)==8 && !strncmp(id,"answer-",7) && id[7]>='0' && id[7]<='3') {
            language_choice=id[7]-'0';
            language_grade(language_options[language_choice]==lang_index);
        }
    }
    return 1;
}
static void language_book(void) {
    ui_paint_heading(tr("Wortschatz"),0);
    for (int r = 0; r < 6 && language_book_page * 6 + r < 24; r++) {
        int i = language_book_page * 6 + r, y = 145 + r * 86;
        drawtext_raw(30, y, v1_lessons[i][language_source()], ui_font(UI_PRIMARY), 540);
        drawtext_raw(30, y + 38, v1_lessons[i][language_target()], ui_font(UI_BODY), 540);
        divider(28, y + 76, 544);
    }
    lumen_pager(745,language_book_page,4);
}

int main(void) {
    srand((unsigned)time(NULL));
    lumen_run(draw,input,NULL);
    return 0;
}
