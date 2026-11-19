#include "lumen_app.h"
#include "lumen_worker.h"

static int page,count,searching,busy;
static struct {char id[64],title[256],source[64];} rows[6];

static const char *word(const char *de,const char *en,const char *fr) {
    return lumen_language==1 ? en : lumen_language==2 ? fr : de;
}
#include "provider.h"
#include "archive.h"

static void list(void) {
    if(!searching)rescan();
    count=searching ? recipes_count17 : saved_count;
    if(page<0 || page>128 || page*6>=count)page=0;
    memset(rows,0,sizeof(rows));
    for(int i=0;i<6 && page*6+i<count;i++) {
        int index=page*6+i;
        if(searching) {
            const Recipe17 *record=&recipes17[index];
            snprintf(rows[i].id,sizeof(rows[i].id),"%s",record->id);
            snprintf(rows[i].title,sizeof(rows[i].title),"%s",record->title);
            if(record->meal)snprintf(rows[i].source,sizeof(rows[i].source),"TheMealDB");
            else snprintf(rows[i].source,sizeof(rows[i].source),"Wikibooks · %s",record->lang);
        } else {
            snprintf(rows[i].id,sizeof(rows[i].id),"%s",saved[index].id);
            snprintf(rows[i].title,sizeof(rows[i].title),"%s",saved[index].title);
        }
    }
}
static void recipe_failed(void) {
    lumen_request("notify","",word("Rezept konnte nicht geladen werden.","Could not load the recipe.","Impossible de charger la recette."),0);
}
static void draw(void) {
    ui_paint_heading(word("Rezepte","Recipes","Recettes"),1);
    lumen_icon_action("search",528,74,IC_SEARCH);
    lumen_text(30,135,540,36,searching ? word("Suchergebnisse","Search results","Résultats") :
        word("Zuletzt geöffnet · offline verfügbar","Recently opened · available offline","Ouvert récemment · disponible hors ligne"),
        PAINT_SECONDARY,-1,1);
    for(int i=0;i<6 && page*6+i<count;i++) {
        int y=189+i*80;
        char id[16];snprintf(id,sizeof(id),"recipe-%d",i);
        if(!busy)lumen_custom_action(id,28,y,544,80);
        ui_paint_icon_button(31,y+18,IC_DISH);
        lumen_text(88,y+6,472,35,rows[i].title,PAINT_BODY,-1,1);
        lumen_text(88,y+41,472,28,*rows[i].source ? rows[i].source :
            word("Offline verfügbar","Available offline","Disponible hors ligne"),PAINT_SECONDARY,-1,1);
        lumen_list_divider(28,y+79,572);
    }
    lumen_pager(715,page,(count+5)/6);
    if(recipe_worker>0)lumen_schedule(250);
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
    if(in->kind==INPUT_BACK && searching){searching=lumen_back=busy=open_on_finish=0;page=count=0;list();return 1;}
    if(in->kind==INPUT_PAGE && !busy){page=in->x;list();return 1;}
    if(in->kind==INPUT_TEXT && in->x==2) {
        char query[256];
        if(lumen_string("text",query,sizeof(query)) && *query) {
            if(start_recipe(query,NULL)) {
                searching=lumen_back=busy=1;
                page=count=recipes_count17=0;
            } else recipe_failed();
        }
        return 1;
    }
    if(in->kind==INPUT_PAUSE){open_on_finish=0;return 1;}
    if(in->kind==INPUT_TICK) {
        int result=finish_recipe();
        if(result) {
            busy=0;
            if(result<0 && open_on_finish)recipe_failed();
            list();
        }
        return 1;
    }
    char id[24];
    if(!lumen_action_read(in,id,sizeof(id),"clicked"))return 0;
    if(!strcmp(id,"search"))
        lumen_request("input",word("Rezepte suchen","Search recipes","Chercher des recettes"),"",2);
    else if(!busy && strlen(id)==8 && !strncmp(id,"recipe-",7) && id[7]>='0' && id[7]<='5') {
        int row=id[7]-'0';
        if(page*6+row<count && *rows[row].id) {
            int index=page*6+row;
            if(searching) {
                busy=start_recipe(NULL,&recipes17[index]);
                if(!busy)recipe_failed();
            } else if(!read_recipe(saved[index].id,saved[index].title,saved[index].legacy))recipe_failed();
        }
    }
    return 1;
}
int main(void){lumen_run(draw,input,NULL);return 0;}
