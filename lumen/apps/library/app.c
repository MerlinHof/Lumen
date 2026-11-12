#include "lumen_app.h"

enum {SHELF,DETAIL,PREFERENCES};
static struct {char id[17],title[384],author[256];int pin,progress;} books[7],item;
static char query[128],selected[17],filename[256],raw_author[192],format[16];
static int view,page,pages,sort,detail_page,detail_total,kilobytes,supported,confirm;
static const char *word(const char *de,const char *en,const char *fr) {
    return lumen_language==1 ? en : lumen_language==2 ? fr : de;
}
static void set_back(void){lumen_back=view!=SHELF || confirm || *query;}
static void load(void) {
    if(view==DETAIL)lumen_service("library","detail",selected,2);
    else {
        char data[160];snprintf(data,sizeof(data),"%d\n%s",page,query);
        lumen_service("library","shelf",data,1);
    }
}
static void action(const char *operation,const char *value) {
    char data[448];snprintf(data,sizeof(data),"%s%s%s",selected,value ? "\n" : "",value ? value : "");
    lumen_service("library",operation,data,!strcmp(operation,"read") ? 4 : 3);
}
static void cover(int x,int y,int w,int h,const char *id){lumen_book_cover(x,y,w,h,id);}
static void shelf(void) {
    int featured=*books[6].id,top=featured ? 419 : 115;
    lumen_text(28,featured ? 366 : 66,420,44,*query ? query :
        word("M E I N E  B Ü C H E R","M Y  B O O K S","M E S  L I V R E S"),PAINT_SECONDARY,-1,1);
    ui_paint_icon_button(468,60,IC_SEARCH);
    ui_paint_icon_button(528,60,IC_SETTINGS);
    if(featured) {
        cover(28,80,152,208,books[6].id);
        ui_paint_hitbox(28,113,544,196);
        lumen_text(204,96,366,36,word("W E I T E R L E S E N","C O N T I N U E  R E A D I N G","R E P R E N D R E"),PAINT_SECONDARY,-1,1);
        lumen_book_label(204,135,366,books[6].title,books[6].author);
        ui_paint_line(206,282,488,282,3,199);
        int progress=books[6].progress;if(progress>100)progress=100;
        if(progress>0)ui_paint_line(206,282,206+282*progress/100,282,3,0);
        char label[16];snprintf(label,sizeof(label),"%d%%",progress);
        lumen_text(495,263,75,39,label,PAINT_BODY,0,0);
    }
    for(int i=0;i<(featured ? 3 : 6);i++)if(*books[i].id) {
        int x=28+i%3*188,y=top+i/3*304;
        cover(x,y,164,224,books[i].id);
        if(books[i].pin)ui_paint_icon_button(x+113,y+4,IC_PIN);
        lumen_wrapped(x+8,y+229,156,48,books[i].title,PAINT_BODY,2);
        lumen_text(x+8,y+273,156,25,books[i].author,PAINT_SECONDARY,-1,1);
        ui_paint_hitbox(x,y,168,300);
    }
    if(!featured && !*books[0].id)lumen_wrapped(28,180,544,115,
        word("Deine Bücher erscheinen hier.","Your books will appear here.","Vos livres apparaîtront ici."),
        PAINT_TITLE,3);
    lumen_pager(745,page,pages);
}
static void draw(void) {
    if(view==PREFERENCES) {
        const char *modes[]={word("Titel","Title","Titre"),word("Autor","Author","Auteur"),
            word("Zuletzt gelesen","Last read","Dernière lecture"),word("Zuletzt hinzugefügt","Recently added","Ajout récent"),
            word("Dateigröße","Size","Taille"),word("Fortschritt","Progress","Progression")};
        ui_paint_heading(word("Bibliothek","Library","Bibliothèque"),0);
        lumen_select_row(203,word("Sortierung","Sort by","Trier par"),modes[sort]);
    } else if(view==DETAIL) {
        ui_paint_heading(word("Buchdetails","Book details","Détails du livre"),0);
        cover(28,145,120,164,selected);
        lumen_book_label(176,151,396,item.title,item.author);
        char details[64];snprintf(details,sizeof(details),"%s · %.1f MB",format,kilobytes/1024.0);
        lumen_text(30,329,540,36,details,PAINT_SECONDARY,-1,1);
        lumen_button(30,385,540,48,supported ? word("Lesen","Read","Lire") :
            word("Im Kindle lesen","Read in Kindle","Lire sur Kindle"),1);
        lumen_button(30,451,260,48,item.pin ? word("Loslösen","Unpin","Désépingler") : word("Anheften","Pin","Épingler"),0);
        lumen_button(310,451,260,48,word("Umbenennen","Rename file","Renommer"),0);
        lumen_button(30,517,260,48,word("Titel bearbeiten","Edit title","Modifier le titre"),0);
        lumen_button(310,517,260,48,word("Autor bearbeiten","Edit author","Modifier l’auteur"),0);
        lumen_button(30,583,260,48,word("Buch löschen","Delete book","Supprimer le livre"),0);
        if(detail_total>0) {
            snprintf(details,sizeof(details),"%d / %d",detail_page+1,detail_total);
            lumen_text(30,661,540,35,details,PAINT_SECONDARY,-1,1);
        }
    } else shelf();
    if(confirm) {
        ui_paint_surface(20,274,560,218,16,255,100);
        ui_paint_text(30,294,540,90,word("Dieses Buch dauerhaft löschen?","Permanently delete this book?",
            "Supprimer définitivement ce livre ?"),PAINT_HEADING);
        lumen_button(36,414,250,52,word("Abbrechen","Cancel","Annuler"),0);
        lumen_button(312,414,250,52,word("Löschen","Delete","Supprimer"),1);
    }
}
static void read_row(int row,int field) {
    char key[32];
    snprintf(key,sizeof(key),"id%d",field);lumen_string(key,books[row].id,sizeof(books[row].id));
    snprintf(key,sizeof(key),"title%d",field);lumen_string(key,books[row].title,sizeof(books[row].title));
    snprintf(key,sizeof(key),"author%d",field);lumen_string(key,books[row].author,sizeof(books[row].author));
    snprintf(key,sizeof(key),"pinned%d",field);books[row].pin=lumen_number(key);
    snprintf(key,sizeof(key),"progress%d",field);books[row].progress=lumen_number(key);
}
static int input(const Input *in) {
    if(in->kind==INPUT_OPEN || in->kind==INPUT_RESUME || in->kind==INPUT_REFRESH){load();return 1;}
    if(in->kind==INPUT_INTENT) {
        char operation[24];
        if(lumen_string("operation",operation,sizeof(operation))) {
            if(!strcmp(operation,"search") && lumen_string("text",query,sizeof(query))) {
                view=SHELF;page=0;set_back();load();
            } else if(!strcmp(operation,"book-detail") && lumen_string("text",selected,sizeof(selected))) {
                view=DETAIL;set_back();load();
            }
        }
        return 1;
    }
    if(in->kind==INPUT_BACK) {
        if(confirm)confirm=0;
        else if(view!=SHELF){view=SHELF;load();}
        else if(*query){query[0]=0;page=0;load();}
        set_back();return 1;
    }
    if(in->kind==INPUT_PAGE && !confirm){page=in->x;load();return 1;}
    if(in->kind==INPUT_SELECT) {
        if(in->x==2 && in->y>=0 && in->y<6) {
            char value[2]={(char)('0'+in->y),0};lumen_service("library","sort",value,3);
        }
        if(in->x==3 && in->y>=0 && in->y<3) {
            if(in->y==0)action("pin",NULL);
            if(in->y==1){view=DETAIL;load();}
            if(in->y==2)confirm=1;
            set_back();
        }
        return 1;
    }
    if(in->kind==INPUT_TEXT) {
        char value[384];
        if(lumen_string("text",value,sizeof(value))) {
            if(in->x==1){snprintf(query,sizeof(query),"%.*s",(int)sizeof(query)-1,value);page=0;set_back();load();}
            if(in->x>=4 && in->x<=6 && *value)action((const char *[]){"rename","title","author"}[in->x-4],value);
        }
        return 1;
    }
    if(in->kind==INPUT_SERVICE) {
        if(in->x==1 && in->y) {
            page=lumen_number("page");pages=lumen_number("pages");sort=lumen_number("sort");
            if(sort<0 || sort>=6)sort=0;
            for(int i=0;i<7;i++)read_row(i,i);
        } else if(in->x==2 && in->y && view==DETAIL) {
            typeof(item) original=books[0];read_row(0,0);item=books[0];books[0]=original;
            lumen_string("filename",filename,sizeof(filename));lumen_string("raw_author",raw_author,sizeof(raw_author));
            lumen_string("format",format,sizeof(format));supported=lumen_number("supported");
            detail_page=lumen_number("page");detail_total=lumen_number("total");kilobytes=lumen_number("kilobytes");
        } else if(in->x==4 && !in->y && view==SHELF){view=DETAIL;set_back();load();}
        else if(in->x==3) {
            if(!in->y)lumen_request("notify","Library",word("Aktion fehlgeschlagen.","Action failed.","Échec de l’action."),0);
            else load();
        }
        return 1;
    }
    if(in->kind!=INPUT_TAP && in->kind!=INPUT_HOLD)return 0;
    int x=in->x,y=in->y;
    if(confirm) {
        if(in->kind==INPUT_TAP && y>=414 && y<466) {
            if(x>=312 && x<562){action("delete",NULL);view=SHELF;}
            confirm=0;set_back();
        }
        return 1;
    }
    if(view==DETAIL && in->kind==INPUT_TAP) {
        if(y>=385 && y<433)action("read",NULL);
        else if(y>=451 && y<499) {
            if(x<300)action("pin",NULL);
            else lumen_request("input",word("Datei umbenennen","Rename file","Renommer"),filename,4);
        } else if(y>=517 && y<565)
            lumen_request("input",x<300 ? word("Titel","Title","Titre") : word("Autor","Author","Auteur"),
                x<300 ? item.title : raw_author,x<300 ? 5 : 6);
        else if(y>=583 && y<631 && x<300){confirm=1;set_back();}
        return 1;
    }
    if(view==PREFERENCES && in->kind==INPUT_TAP && y>=203 && y<251 && x>=360) {
        const char *modes[]={word("Titel","Title","Titre"),word("Autor","Author","Auteur"),
            word("Zuletzt gelesen","Last read","Dernière lecture"),word("Zuletzt hinzugefügt","Recently added","Ajout récent"),
            word("Dateigröße","Size","Taille"),word("Fortschritt","Progress","Progression")};
        lumen_select(modes,6,sort,2);return 1;
    }
    if(view!=SHELF)return 0;
    if(in->kind==INPUT_TAP && y>=60 && y<104 && x>=468) {
        if(x>=528){view=PREFERENCES;set_back();}
        else lumen_request("input",word("Bücher suchen","Search books","Chercher des livres"),query,1);
        return 1;
    }
    int index=-1,featured=*books[6].id,top=featured ? 419 : 115;
    if(featured && x>=28 && x<572 && y>=80 && y<310)index=6;
    else if(x>=28 && x<572 && y>=top && y<742 && (x-28)%188<168)
        index=(y-top)/304*3+(x-28)/188;
    if(index<0 || index>=7 || !*books[index].id)return 0;
    snprintf(selected,sizeof(selected),"%s",books[index].id);
    if(in->kind==INPUT_HOLD) {
        const char *modes[]={books[index].pin ? word("Loslösen","Unpin","Désépingler") :
            word("Anheften","Pin","Épingler"),word("Buchdetails","Book details","Détails"),
            word("Löschen","Delete","Supprimer")};
        lumen_select(modes,3,-1,3);
    } else action("read",NULL);
    return 1;
}
int main(void){lumen_run(draw,input,NULL);return 0;}
