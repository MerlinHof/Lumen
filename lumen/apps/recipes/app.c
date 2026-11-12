#include "lumen_app.h"

static int page,count,searching,busy;
static struct {char id[64],title[256],source[64];} rows[6];

static const char *word(const char *de,const char *en,const char *fr) {
    return lumen_language==1 ? en : lumen_language==2 ? fr : de;
}
static void list(void) {
    char number[16];snprintf(number,sizeof(number),"%d",page);
    lumen_service("recipes",searching ? "results" : "archive",number,1);
}
static void draw(void) {
    ui_paint_heading(word("Rezepte","Recipes","Recettes"),1);
    ui_paint_icon_button(528,74,IC_SEARCH);
    lumen_text(30,135,540,36,searching ? word("Suchergebnisse","Search results","Résultats") :
        word("Zuletzt geöffnet · offline verfügbar","Recently opened · available offline","Ouvert récemment · disponible hors ligne"),
        PAINT_SECONDARY,-1,1);
    for(int i=0;i<6 && page*6+i<count;i++) {
        int y=189+i*80;
        ui_paint_hitbox(28,y,544,80);
        ui_paint_icon_button(31,y+18,IC_DISH);
        lumen_text(88,y+6,472,35,rows[i].title,PAINT_BODY,-1,1);
        lumen_text(88,y+41,472,28,*rows[i].source ? rows[i].source :
            word("Offline verfügbar","Available offline","Disponible hors ligne"),PAINT_SECONDARY,-1,1);
        ui_paint_line(28,y+79,572,y+79,1,190);
    }
    lumen_pager(715,page,(count+5)/6);
    if(busy) {
        ui_paint_surface(60,292,480,146,12,255,90);
        ui_paint_text(70,315,460,100,word("Rezepte werden geladen …","Loading recipes…","Chargement des recettes…"),PAINT_HEADING);
    } else if(!count) {
        lumen_wrapped(30,260,540,115,searching ?
            word("Keine Rezepte gefunden.","No recipes found.","Aucune recette trouvée.") :
            word("Über die Lupe nach einem Gericht suchen.","Tap search to find a dish.","Touchez la loupe pour chercher un plat."),
            PAINT_HEADING,3);
    }
}
static int input(const Input *in) {
    if(in->kind==INPUT_OPEN || in->kind==INPUT_RESUME || in->kind==INPUT_REFRESH){list();return 1;}
    if(in->kind==INPUT_BACK && searching){searching=lumen_back=busy=0;page=count=0;list();return 1;}
    if(in->kind==INPUT_PAGE && !busy){page=in->x;list();return 1;}
    if(in->kind==INPUT_TEXT && in->x==2) {
        char query[256];
        if(lumen_string("text",query,sizeof(query)) && *query) {
            searching=lumen_back=busy=1;page=count=0;
            lumen_service("recipes","search",query,3);
        }
        return 1;
    }
    if(in->kind==INPUT_SERVICE) {
        if(in->x==1 && in->y) {
            count=lumen_number("count");page=lumen_number("page");busy=lumen_number("busy");
            for(int i=0;i<6;i++) {
                char field[16];
                snprintf(field,sizeof(field),"id%d",i);lumen_string(field,rows[i].id,sizeof(rows[i].id));
                snprintf(field,sizeof(field),"title%d",i);lumen_string(field,rows[i].title,sizeof(rows[i].title));
                snprintf(field,sizeof(field),"source%d",i);lumen_string(field,rows[i].source,sizeof(rows[i].source));
            }
        } else if((in->x==3 || in->x==4) && !in->y)busy=0;
        return 1;
    }
    if(in->kind!=INPUT_TAP)return 0;
    if(in->x>=528 && in->x<572 && in->y>=74 && in->y<118)
        lumen_request("input",word("Rezepte suchen","Search recipes","Chercher des recettes"),"",2);
    else if(!busy && in->x>=28 && in->x<572 && in->y>=189 && in->y<669) {
        int row=(in->y-189)/80;
        if(page*6+row<count && *rows[row].id) {
            busy=searching;
            lumen_service("recipes",searching ? "fetch" : "open",rows[row].id,4);
        }
    }
    return 1;
}
int main(void){lumen_run(draw,input,NULL);return 0;}
