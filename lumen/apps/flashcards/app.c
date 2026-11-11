#include "lumen_app.h"
#include "lumen_fsrs.h"
#include <math.h>

typedef struct {
    uint64_t id;
    char deck[64],front[512],back[1024];
    MemoryCard memory;
} Flashcard;
static Flashcard cards[512];
static int count,selected=-1,revealed,browse,page,editing=-1;
static char deck[64]="General",front[512];

enum { EDIT_DECK=1, EDIT_FRONT, EDIT_BACK, DELETE_CARD };

static const char *word(const char *de,const char *en,const char *fr) {
    return lumen_language==1 ? en : lumen_language==2 ? fr : de;
}
static void save(void) {
    app_state_save("cards.bin",cards,sizeof(cards));
}
static void load(void) {
    if(!app_state_load("cards.bin",cards,sizeof(cards))) {
        static unsigned char legacy[sizeof(cards)+8];
        if(lumen_legacy_state("flashcards-1.bin",legacy,sizeof(legacy))) {
            uint32_t header[2];memcpy(header,legacy,8);
            if(header[0]==0x4c554d16 && header[1]==sizeof(cards)) {
                memcpy(cards,legacy+8,sizeof(cards));
                save();
            }
        }
    }
    for(int i=0;i<512;i++) {
        Flashcard *card=&cards[i];
        card->deck[63]=card->front[511]=card->back[1023]=0;
        if(!*card->front)continue;
        MemoryCard *m=&card->memory;
        if(!isfinite(m->stability) || !isfinite(m->difficulty) ||
           m->stability<0 || m->stability>36500 || m->difficulty<0 || m->difficulty>10 || m->due<0)
            memset(m,0,sizeof(*m));
        cards[count++]=*card;
    }
    memset(cards+count,0,(512-count)*sizeof(*cards));
}
static void next_card(void) {
    selected=-1;revealed=0;
    int64_t now=time(NULL);
    for(int i=0;i<count;i++)
        if(cards[i].memory.due<=now && (selected<0 || cards[i].memory.due<cards[selected].memory.due))
            selected=i;
}
static void edit(int index) {
    editing=index;
    lumen_request("input",word("Sammlung","Deck","Paquet"),
                  index>=0 ? cards[index].deck : deck,EDIT_DECK);
}
static void review(int rating) {
    Flashcard *card=&cards[selected];
    fsrs_review(&card->memory,rating,time(NULL));
    save();
    char path[2048];
    if(lumen_storage_path("reviews.tsv",path,sizeof(path))) {
        FILE *file=fopen(path,"a");
        if(file) {
            const MemoryCard *m=&card->memory;
            fprintf(file,"%lld\t%llu\t%d\t%.8g\t%.8g\t%lld\n",(long long)m->last,
                    (unsigned long long)card->id,rating,m->stability,m->difficulty,(long long)m->due);
            fclose(file);
        }
    }
    next_card();
}
static void draw(void) {
    lumen_back=browse;
    ui_paint_heading(browse ? word("Kartenbibliothek","Card library","Bibliothèque de cartes") :
                     word("Karteikarten","Flashcards","Cartes mémoire"),browse ? 1 : 2);
    ui_paint_icon_button(528,74,IC_PLUS);
    if(browse) {
        for(int row=0;row<7 && page*7+row<count;row++) {
            Flashcard *card=&cards[page*7+row];
            int y=151+row*72;
            ui_paint_hitbox(28,y,480,66);
            lumen_text(30,y,466,32,card->front,PAINT_BODY,-1,1);
            lumen_text(30,y+33,466,30,card->deck,PAINT_SECONDARY,-1,1);
            ui_paint_icon_button(522,y+8,IC_TRASH);
            ui_paint_line(28,y+66,572,y+66,1,190);
        }
        lumen_pager(695,page,(count+6)/7);
        return;
    }
    ui_paint_icon_button(468,74,IC_CONTENTS);
    int due=0;
    int64_t now=time(NULL);
    for(int i=0;i<count;i++)due+=cards[i].memory.due<=now;
    char label[96];snprintf(label,sizeof(label),"%d · %d %s · FSRS-6",count,due,
                            word("Fällig","due","à revoir"));
    ui_paint_text(28,136,544,36,label,PAINT_SECONDARY);
    if(selected<0) {
        ui_paint_text(28,280,544,70,word("Für heute geschafft.","Done for today.","Terminé pour aujourd’hui."),PAINT_TITLE);
        lumen_wrapped(40,385,520,125,count ?
            word("Die nächste Karte erscheint, sobald sie fällig ist.","The next card appears when it is due.",
                 "La prochaine carte apparaîtra à la date prévue.") :
            word("Mit + eine Karte erstellen.","Tap + to create a card.","Touchez + pour créer une carte."),PAINT_BODY,4);
        return;
    }
    Flashcard *card=&cards[selected];
    lumen_text(32,215,536,32,card->deck,PAINT_SECONDARY,-1,1);
    lumen_wrapped(32,275,536,150,card->front,PAINT_TITLE,5);
    if(!revealed) {
        lumen_button(28,622,544,60,word("Antwort zeigen","Show answer","Afficher la réponse"),0);
        return;
    }
    ui_paint_line(28,440,572,440,1,190);
    lumen_wrapped(32,470,536,180,card->back,PAINT_BODY,6);
    const char *labels[]={word("Nochmal","Again","À revoir"),word("Schwer","Hard","Difficile"),
                          word("Gut","Good","Bien"),word("Leicht","Easy","Facile")};
    for(int i=0;i<4;i++) {
        MemoryCard preview=card->memory;
        fsrs_review(&preview,i+1,now);
        long seconds=(long)(preview.due-now);
        snprintf(label,sizeof(label),seconds<3600 ? "%ld min" : seconds<86400 ? "%ld h" : "%ld d",
                 seconds<3600 ? seconds/60 : seconds<86400 ? seconds/3600 : seconds/86400);
        ui_paint_text(28+i*140,687,124,30,label,PAINT_SECONDARY);
        lumen_button(28+i*140,722,124,54,labels[i],0);
    }
}
static int input(const Input *in) {
    if(in->kind==INPUT_OPEN || in->kind==INPUT_RESUME){next_card();return 1;}
    if(in->kind==INPUT_BACK){browse=0;next_card();return 1;}
    if(in->kind==INPUT_PAGE){page=in->x;return 1;}
    if(in->kind==INPUT_SELECT && in->x==DELETE_CARD) {
        if(in->y==1 && editing>=0 && editing<count) {
            memmove(cards+editing,cards+editing+1,(count-editing-1)*sizeof(*cards));
            memset(cards+--count,0,sizeof(*cards));
            if(page*7>=count && page)page--;
            save();next_card();
        }
        editing=-1;return 1;
    }
    if(in->kind==INPUT_TEXT) {
        char text[1024];
        if(!lumen_string("text",text,sizeof(text)) || !*text)return 1;
        if(in->x==EDIT_DECK) {
            snprintf(deck,sizeof(deck),"%.*s",(int)sizeof(deck)-1,text);
            lumen_request("input",word("Frage","Question","Question"),
                          editing>=0 ? cards[editing].front : "",EDIT_FRONT);
        } else if(in->x==EDIT_FRONT) {
            snprintf(front,sizeof(front),"%.*s",(int)sizeof(front)-1,text);
            lumen_request("input",word("Antwort","Answer","Réponse"),
                          editing>=0 ? cards[editing].back : "",EDIT_BACK);
        } else if(in->x==EDIT_BACK && *front && (editing>=0 || count<512)) {
            int index=editing>=0 ? editing : count++;
            Flashcard *card=&cards[index];
            if(!card->id)card->id=(uint64_t)time(NULL)*1000+index;
            snprintf(card->deck,sizeof(card->deck),"%s",deck);
            snprintf(card->front,sizeof(card->front),"%s",front);
            snprintf(card->back,sizeof(card->back),"%s",text);
            editing=-1;save();next_card();
        }
        return 1;
    }
    if(in->kind!=INPUT_TAP)return 0;
    int x=in->x,y=in->y;
    if(y>=74 && y<118 && x>=528 && x<572) {
        if(count<512)edit(-1);
        else lumen_request("notify","",word("Kartenbibliothek voll.","Card library full.","Bibliothèque pleine."),0);
    } else if(!browse && y>=74 && y<118 && x>=468 && x<512) {
        browse=1;page=0;
    } else if(browse && y>=151 && y<655 && x>=28 && x<572) {
        int index=page*7+(y-151)/72;
        if(index<count) {
            if(x<510)edit(index);
            else {
                editing=index;
                const char *options[]={word("Abbrechen","Cancel","Annuler"),word("Karte löschen","Delete card","Supprimer la carte")};
                lumen_select(options,2,-1,DELETE_CARD);
            }
        }
    } else if(!browse && selected>=0 && x>=28 && x<572) {
        if(!revealed && y>=622 && y<682)revealed=1;
        else if(revealed && y>=722 && y<776 && (x-28)%140<124)review(1+(x-28)/140);
    }
    return 1;
}
int main(void) {
    load();
    lumen_run(draw,input,NULL);
    return 0;
}
