#include "lumen_app.h"

enum {BOOK,CONTENTS,SETTINGS};
static int view,chrome,page,total,preferences[10],settings_page,category,list_page,count,confirm=-1;
static char title[384],labels[8][768];
static int positions[8];
static const char *word(const char *de,const char *en,const char *fr) {
    return lumen_language==1 ? en : lumen_language==2 ? fr : de;
}
static void request(const char *operation,const char *data,int action) {
    lumen_service("document",operation,data,action);
}
static void state(void){request("state","",1);}
static void contents(void) {
    char value[32];snprintf(value,sizeof(value),"%d %d",category,list_page);
    request("contents",value,2);
}
static void number(const char *operation,int value,int action) {
    char data[24];snprintf(data,sizeof(data),"%d",value);request(operation,data,action);
}
static void setting(int id,int value) {
    char data[32];snprintf(data,sizeof(data),"%d %d",id,value);request("setting",data,1);
}
static void show(int next) {
    view=next;lumen_back=view!=BOOK;
    if(view==CONTENTS){list_page=0;contents();}
    else state();
}
static const char *setting_label(int i) {
    static const char *names[][3]={
      {"Schriftfamilie","Typeface","Police"},{"Schriftgröße","Font size","Taille du texte"},
      {"Zeilenabstand","Line spacing","Interligne"},{"Seitenrand","Margins","Marges"},
      {"Ausrichtung","Alignment","Alignement"},{"Absatzabstand","Paragraph spacing","Espacement des paragraphes"},
      {"Zeichenabstand","Letter spacing","Espacement des lettres"},{"Wischen zum Blättern","Swipe to turn pages","Balayer pour tourner"},
      {"Tippzonen","Tap zones","Zones tactiles"},{"Lesefortschritt","Progress indicator","Progression"}};
    return word(names[i][0],names[i][1],names[i][2]);
}
static const char **choices(int id,int *count) {
    static const char *fonts[]={"Charis","Sans Serif","Monospace","Times","Noto Serif"};
    static const char *values[7];
    if(id==0){*count=5;return fonts;}
    if(id==4) {
        *count=2;values[0]=word("Linksbündig","Left aligned","Aligné à gauche");
        values[1]=word("Blocksatz","Justified","Justifié");
    } else if(id==8) {
        *count=2;values[0]=word("Links / rechts","Left / right","Gauche / droite");
        values[1]=word("Mehr Platz vorwärts","More room forward","Plus d’espace pour avancer");
    } else if(id==9) {
        *count=7;
        values[0]=word("Standard","Default","Par défaut");values[1]=word("Seitenpfeile","Page arrows","Flèches");
        values[2]=word("Prozent","Percentage","Pourcentage");values[3]=word("Seitenzahl","Page number","Numéro de page");
        values[4]=word("Restzeit","Time remaining","Temps restant");values[5]=word("Keine Anzeige","Hidden","Masqué");
        values[6]=word("Fortschrittsbalken","Progress bar","Barre de progression");
    } else return NULL;
    return values;
}
static void draw_settings(void) {
    ui_paint_heading(word("Lesen","Reading","Lecture"),0);
    lumen_text(30,113,540,35,settings_page ? word("Abstände & Gesten","Spacing and gestures","Espacement et gestes") :
        word("Schrift & Satz","Typography","Typographie"),PAINT_SECONDARY,-1,1);
    for(int row=0;row<8 && settings_page*8+row<10;row++) {
        int id=settings_page*8+row,y=155+row*68,n=0;
        const char **options=choices(id,&n);
        if(options)lumen_select_row(y,setting_label(id),options[preferences[id]>=0 && preferences[id]<n ? preferences[id] : 0]);
        else {
            lumen_text(30,y,351,48,setting_label(id),PAINT_BODY,0,1);
            if(id==7)lumen_toggle(492,y+6,preferences[id]);
            else {
                char value[16];
                if(id==2)snprintf(value,sizeof(value),"%.2f",preferences[id]/100.0);
                else snprintf(value,sizeof(value),"%d",preferences[id]);
                lumen_button(394,y,48,44,"−",0);lumen_button(524,y,48,44,"+",0);
                ui_paint_text(443,y,81,44,value,PAINT_BODY);
            }
            ui_paint_line(28,y+62,572,y+62,1,190);
        }
    }
    lumen_pager(745,settings_page,2);
}
static void draw(void) {
    lumen_fullscreen=view==BOOK && !chrome && total>0;
    if(view==SETTINGS){draw_settings();return;}
    if(view==CONTENTS) {
        ui_paint_heading(word("Inhalt","Contents","Sommaire"),1);
        ui_paint_icon_button(528,74,IC_PIN);
        const char *tabs[]={word("Kapitel","Chapters","Chapitres"),word("Lesezeichen","Bookmarks","Signets"),
            word("Markierungen","Highlights","Surlignages")};
        lumen_segments("contents",28,145,544,48,tabs,3,category);
        for(int row=0;row<8 && list_page*8+row<count;row++) {
            int y=213+row*62;
            lumen_wrapped(30,y+5,category==2 ? 480 : 535,49,labels[row],PAINT_BODY,2);
            if(category==2)ui_paint_icon_button(522,y+8,IC_TRASH);
            ui_paint_hitbox(28,y,544,61);
            ui_paint_line(28,y+61,572,y+61,1,190);
        }
        if(!count)ui_paint_text(28,264,544,70,word("Noch keine Einträge.","No entries yet.","Aucune entrée."),PAINT_BODY);
        lumen_pager(745,list_page,(count+7)/8);
        if(confirm>=0) {
            ui_paint_surface(24,275,552,220,16,255,100);
            ui_paint_text(35,295,530,85,word("Markierung löschen?","Delete highlight?","Supprimer le surlignage ?"),PAINT_HEADING);
            lumen_button(38,412,250,52,word("Abbrechen","Cancel","Annuler"),0);
            lumen_button(312,412,250,52,word("Löschen","Delete","Supprimer"),1);
        }
        return;
    }
    if(!total) {
        ui_paint_heading(word("Lesen","Reader","Lecture"),0);
        lumen_button(28,298,544,54,word("Weiterlesen","Continue reading","Reprendre la lecture"),1);
        lumen_button(28,369,544,54,word("Bibliothek","Library","Bibliothèque"),0);
        return;
    }
    lumen_document_page(chrome);
    if(chrome) {
        ui_paint_surface(0,56,600,77,0,255,-1);
        lumen_text(28,74,420,44,title,PAINT_HEADING,0,1);
        ui_paint_icon_button(468,74,IC_CONTENTS);
        ui_paint_icon_button(528,74,IC_SETTINGS);
        ui_paint_line(0,130,599,130,1,174);
        return;
    }
    int mode=preferences[9];char indicator[36];
    if(mode==1)lumen_pager(746,page,total);
    else if(mode!=5) {
        if(mode==2)snprintf(indicator,sizeof(indicator),"%d%%",total>1 ? page*100/(total-1) : 100);
        else if(mode==4)snprintf(indicator,sizeof(indicator),"~ %d min",(total-page-1)*2);
        else snprintf(indicator,sizeof(indicator),"%d / %d",page+1,total);
        if(mode!=6)lumen_text(mode==3 ? 392 : 180,746,mode==3 ? 180 : 240,35,indicator,PAINT_SECONDARY,mode==3 ? 130 : 0,0);
        if(mode==0 || mode==6) {
            ui_paint_line(28,787,571,787,3,204);
            ui_paint_line(28,787,28+543*(page+1)/total,787,3,0);
        }
    }
}
static int input(const Input *in) {
    if(in->kind==INPUT_OPEN || in->kind==INPUT_RESUME || in->kind==INPUT_REFRESH){if(view==CONTENTS)contents();else state();return 1;}
    if(in->kind==INPUT_INTENT) {
        view=BOOK;chrome=0;lumen_back=0;state();return 1;
    }
    if(in->kind==INPUT_BACK && view!=BOOK){if(confirm>=0)confirm=-1;else show(BOOK);return 1;}
    if(in->kind==INPUT_PAGE) {
        if(view==SETTINGS)settings_page=in->x;
        else if(view==CONTENTS){list_page=in->x;contents();}
        else if(total)number("goto",in->x,1);
        return 1;
    }
    if(in->kind==INPUT_SELECT && view==SETTINGS && in->x>=10 && in->x<20 && in->y>=0)
        setting(in->x-10,in->y);
    if(in->kind==INPUT_SERVICE) {
        if(in->y) {
            page=lumen_number("page");total=lumen_number("total");
            lumen_string("title",title,sizeof(title));
            for(int i=0;i<10;i++){char key[24];snprintf(key,sizeof(key),"setting%d",i);preferences[i]=lumen_number(key);}
            if(in->x==2) {
                count=lumen_number("count");list_page=lumen_number("list_page");
                for(int i=0;i<8;i++){
                    char key[24];snprintf(key,sizeof(key),"label%d",i);lumen_string(key,labels[i],sizeof(labels[i]));
                    snprintf(key,sizeof(key),"position%d",i);positions[i]=lumen_number(key);
                }
            } else if(in->x==3 && view==CONTENTS)contents();
        }
        return 1;
    }
    if(in->kind==INPUT_COMPONENT && view==CONTENTS) {
        char value[8];lumen_string("text",value,sizeof(value));
        if(value[0]>='0' && value[0]<='2'){category=value[0]-'0';list_page=0;contents();}
        return 1;
    }
    if(in->kind==INPUT_SWIPE && view==BOOK && !chrome && total && preferences[7]){
        number("turn",in->dx<0 ? 1 : -1,1);return 1;
    }
    if(in->kind!=INPUT_TAP)return 0;
    int x=in->x,y=in->y;
    if(view==SETTINGS) {
        if(y>=155 && y<699) {
            int row=(y-155)/68,id=settings_page*8+row;
            if(id>=10 || (y-155)%68>=48)return 1;
            int n=0;const char **options=choices(id,&n);
            if(options){if(x>=360)lumen_select(options,n,preferences[id],10+id);}
            else if(id==7){if(x>=484)setting(id,!preferences[id]);}
            else {
                int delta=x>=524 ? 1 : x>=394 && x<442 ? -1 : 0;
                const int step[]={0,2,5,3,0,4,2},lo[]={0,16,110,0,0,0,0},hi[]={0,34,190,30,0,24,20};
                int value=preferences[id]+step[id]*delta;
                if(value<lo[id])value=lo[id];if(value>hi[id])value=hi[id];
                if(delta)setting(id,value);
            }
        }
    } else if(view==CONTENTS) {
        if(confirm>=0) {
            if(y>=412 && y<464){if(x>=312)number("remove_annotation",confirm,3);confirm=-1;}
        } else if(y>=74 && y<118 && x>=528)request("bookmark","",3);
        else if(y>=213 && y<709) {
            int row=(y-213)/62;
            if(list_page*8+row<count) {
                if(category==2 && x>=522)confirm=positions[row];
                else {view=BOOK;chrome=lumen_back=0;number(category==2 ? "annotation" : "goto",positions[row],1);}
            }
        }
    } else if(!total) {
        if(y>=298 && y<352)request("resume_book","",1);
        else if(y>=369 && y<423)lumen_open("lumen.library");
    } else if(chrome) {
        if(y>=74 && y<118 && x>=468) {
            if(x>=528){settings_page=0;show(SETTINGS);}
            else show(CONTENTS);
        } else chrome=0;
    } else if(y<78 || y>=722)chrome=1;
    else number("turn",x<(preferences[8] ? 80 : 200) ? -1 : 1,1);
    return 1;
}
int main(void){lumen_run(draw,input,NULL);return 0;}
