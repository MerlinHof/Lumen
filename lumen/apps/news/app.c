#include "lumen_app.h"

static int articles,page,edition_page,count,today,date,busy;
static char query[256];
static struct {char id[17],title[256],detail[180];} rows[7];

static const char *word(const char *de,const char *en,const char *fr) {
    return lumen_language==1 ? en : lumen_language==2 ? fr : de;
}
static void list(void) {
    char number[16];snprintf(number,sizeof(number),"%d",page);
    lumen_service("news",articles ? "articles" : "editions",number,articles ? 2 : 1);
}
static void date_label(char *out,size_t size,int value,const char *suffix) {
    snprintf(out,size,"%02d.%02d.%04d%s",value%100,value/100%100,value/10000,suffix);
}
static void draw(void) {
    ui_paint_heading("News",1);
    ui_paint_icon_button(528,74,IC_SEARCH);
    const char *headline=word("Kuratierte Ausgaben","Curated editions","Éditions sélectionnées");
    char edition[64];
    if(articles) {
        date_label(edition,sizeof(edition),date,"");
        headline=busy ? word("Nachrichten werden geladen …","Loading news…","Chargement des actualités…") :
            *query ? query : edition;
    }
    lumen_text(30,128,540,35,headline,PAINT_SECONDARY,-1,1);
    int top=articles ? 169 : 181, height=articles ? 110 : 75, per_page=articles ? 5 : 7;
    for(int i=0;i<per_page && page*per_page+i<count;i++) {
        int y=top+i*height;
        ui_paint_hitbox(28,y,544,height);
        if(articles) {
            lumen_wrapped(36,y+9,522,66,rows[i].title,PAINT_BODY,2);
            lumen_text(36,y+75,522,25,rows[i].detail,PAINT_SECONDARY,-1,1);
        } else {
            int value=atoi(rows[i].id),current=value==today;
            date_label(edition,sizeof(edition),value,current ? word(" · Heute"," · Today"," · Aujourd’hui") : "");
            lumen_text(36,y+7,470,34,edition,PAINT_BODY,-1,1);
            lumen_text(36,y+41,470,25,current ?
                word("Nachrichten des Tages","Today’s news","Actualités du jour") :
                word("Gespeicherte Ausgabe","Saved edition","Édition enregistrée"),PAINT_SECONDARY,-1,1);
            ui_paint_icon_button(522,y+15,IC_NEXT);
        }
        ui_paint_line(28,y+height-1,572,y+height-1,1,190);
    }
    if(articles && !count && !busy)
        lumen_wrapped(30,210,540,120,word("Keine Artikel verfügbar.","No articles available.",
            "Aucun article disponible."),PAINT_HEADING,3);
    lumen_pager(746,page,(count+per_page-1)/per_page);
}
static void show_articles(const char *operation,const char *data) {
    if(!articles)edition_page=page;
    articles=lumen_back=1;page=count=0;busy=1;
    lumen_service("news",operation,data,3);
}
static int input(const Input *in) {
    if(in->kind==INPUT_OPEN || in->kind==INPUT_RESUME || in->kind==INPUT_REFRESH){list();return 1;}
    if(in->kind==INPUT_BACK && articles) {
        articles=lumen_back=busy=count=0;page=edition_page;list();return 1;
    }
    if(in->kind==INPUT_PAGE){page=in->x;list();return 1;}
    if(in->kind==INPUT_TEXT && in->x==1 && lumen_string("text",query,sizeof(query)) && *query)
        show_articles("search",query);
    if(in->kind==INPUT_SERVICE) {
        if(in->x==(articles ? 2 : 1) && in->y) {
            page=lumen_number("page");count=lumen_number("count");
            today=lumen_number("today");date=lumen_number("date");busy=articles && lumen_number("busy");
            if(articles)lumen_string("query",query,sizeof(query));
            for(int i=0;i<7;i++) {
                char field[20];
                snprintf(field,sizeof(field),"id%d",i);lumen_string(field,rows[i].id,sizeof(rows[i].id));
                snprintf(field,sizeof(field),"title%d",i);lumen_string(field,rows[i].title,sizeof(rows[i].title));
                snprintf(field,sizeof(field),"detail%d",i);lumen_string(field,rows[i].detail,sizeof(rows[i].detail));
            }
        } else if(in->x==3 || in->x==4)list();
        return 1;
    }
    if(in->kind!=INPUT_TAP)return 0;
    if(in->x>=528 && in->x<572 && in->y>=74 && in->y<118) {
        lumen_request("input",word("News suchen","Search news","Rechercher des actualités"),query,1);
        return 1;
    }
    int top=articles ? 169 : 181,height=articles ? 110 : 75,per_page=articles ? 5 : 7;
    if(in->x>=28 && in->x<572 && in->y>=top && in->y<top+height*per_page) {
        int row=(in->y-top)/height;
        if(page*per_page+row<count && *rows[row].id) {
            if(!articles)show_articles("edition",rows[row].id);
            else if(!busy){busy=1;lumen_service("news","open",rows[row].id,4);}
        }
    }
    return 1;
}
int main(void){lumen_run(draw,input,NULL);return 0;}
