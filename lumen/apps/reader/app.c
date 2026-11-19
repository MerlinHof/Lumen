#include "lumen_app.h"

enum {BOOK,CONTENTS,SETTINGS,CONTEXT,SELECTION};
static int context_action=100,mention_state,wiki_state,mention_page,mention_offset,mention_length,mention_expanded;
static char selected_text[2048];
static char context_term[129],mention_quote[1200],wiki_title[180],wiki_extract[2600];
static int private_preferences=1,preferences_loaded;
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
static int valid_preferences(const int values[3]) {
    return values[0]>=0 && values[0]<=1 && values[1]>=0 && values[1]<=1 &&
           values[2]>=0 && values[2]<=6;
}
static void setting(int id,int value) {
    if(private_preferences && id>=7 && id<=9) {
        int values[3];
        memcpy(values,preferences+7,sizeof(values));
        values[id-7]=value;
        if(valid_preferences(values) && app_state_save("reader-controls",values,sizeof(values))) {
            memcpy(preferences+7,values,sizeof(values));
            preferences_loaded=1;
        }
        return;
    }
    char data[32];snprintf(data,sizeof(data),"%d %d",id,value);request("setting",data,1);
}
static void import_controls(void) {
    if(!valid_preferences(preferences+7)) {
        preferences[7]=1;preferences[8]=0;preferences[9]=0;
    }
    if(preferences[9]!=5) {
        char text[32];size_t length;
        if(lumen_legacy_read("reader-progress15.txt",text,sizeof(text)-1,&length)) {
            text[length]=0;
            char *end;
            long value=strtol(text,&end,10);
            if(end!=text && value>=0 && value<=6)preferences[9]=(int)value;
        }
    }
    preferences_loaded=1;
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
        char key[16];snprintf(key,sizeof(key),"setting-%d",id);
        if(options)lumen_select_action(key,y,setting_label(id),options[preferences[id]>=0 && preferences[id]<n ? preferences[id] : 0]);
        else {
            lumen_text(30,y,351,48,setting_label(id),PAINT_BODY,0,1);
            if(id==7) {
                lumen_toggle(492,y+6,preferences[id]);lumen_custom_action(key,470,y,102,48);
            }
            else {
                char value[16];
                if(id==2)snprintf(value,sizeof(value),"%.2f",preferences[id]/100.0);
                else snprintf(value,sizeof(value),"%d",preferences[id]);
                snprintf(key,sizeof(key),"decrease-%d",id);lumen_button_action(key,394,y,48,44,"−",0);
                snprintf(key,sizeof(key),"increase-%d",id);lumen_button_action(key,524,y,48,44,"+",0);
                ui_paint_text(443,y,81,44,value,PAINT_BODY);
            }
            lumen_list_divider(28,y+62,572);
        }
    }
    lumen_pager(745,settings_page,2);
}
static void context_open(const char *text,int lookup) {
    size_t used=strcspn(text,"\n\r");
    if(used>sizeof(context_term)-1)used=sizeof(context_term)-1;
    while(used && ((unsigned char)text[used]&0xc0)==0x80)used--;
    snprintf(context_term,sizeof(context_term),"%.*s",(int)used,text);
    size_t size=strlen(context_term);
    while(size && strchr(".,;:!? ",context_term[size-1]))context_term[--size]=0;
    if(!size)return;
    context_action=context_action>=99000 ? 100 : context_action+2;
    mention_state=1;wiki_state=mention_page=0;
    mention_expanded=0;
    mention_quote[0]=wiki_title[0]=wiki_extract[0]=0;
    view=CONTEXT;lumen_back=1;
    request("first_mention",context_term,context_action);
    if(lookup) {
        wiki_state=1;
        lumen_app_service("lumen.wikipedia","summary",context_term,context_action+1);
    }
}
static void context_draw(void) {
    if(mention_expanded) {
        lumen_form(word("Erste Erwähnung","First mention","Première mention"));
        char page_label[48];
        snprintf(page_label,sizeof(page_label),"%s %d",word("Seite","Page","Page"),mention_page);
        lumen_form_text(page_label,PAINT_SECONDARY);
        lumen_form_excerpt(mention_quote,mention_offset,mention_length);
        lumen_component_button("context-passage",word("Zurück zur Suche","Back to lookup","Retour à la recherche"),0);
        return;
    }
    lumen_document_page(0);
    ui_paint_surface(20,124,560,616,16,255,105);
    lumen_text(40,165,470,50,context_term,PAINT_HEADING,0,1);
    lumen_icon_action("context-close",515,159,IC_CLOSE);
    lumen_list_divider(40,222,560);
    char label[96];
    snprintf(label,sizeof(label),"%s",word("Erste Erwähnung","First mention","Première mention"));
    lumen_text(40,235,520,31,label,PAINT_SECONDARY,-1,1);
    if(mention_state==2) {
        lumen_excerpt(40,273,515,235,mention_quote,9,mention_offset,mention_length);
        snprintf(label,sizeof(label),"%s %d",word("Seite","Page","Page"),mention_page);
        snprintf(label+strlen(label),sizeof(label)-strlen(label),"  ›");
        lumen_button_action("context-passage",402,229,156,36,label,0);
    } else lumen_wrapped(40,273,515,95,mention_state==1 ?
        word("Buch wird durchsucht …","Searching the book…","Recherche dans le livre…") :
        word("Keine passende Textstelle gefunden.","No matching passage found.","Aucun passage correspondant."),
        PAINT_BODY,3);
    lumen_list_divider(40,511,560);
    lumen_text(40,522,520,33,"Wikipedia",PAINT_SECONDARY,-1,1);
    if(wiki_state==2) {
        lumen_text(40,553,520,36,wiki_title,PAINT_HEADING,0,1);
        lumen_wrapped(40,592,515,109,wiki_extract,PAINT_BODY,4);
        lumen_text(40,708,520,22,"Wikipedia · CC BY-SA",PAINT_SECONDARY,-1,1);
    } else if(wiki_state==1)lumen_wrapped(40,574,515,92,
        word("Artikel wird geladen …","Looking up the article…","Recherche de l’article…"),PAINT_BODY,3);
    else {
        if(wiki_state==3)lumen_text(40,566,520,32,word("Kein Artikel verfügbar.",
            "No article available.","Aucun article disponible."),PAINT_SECONDARY,-1,1);
        lumen_button_action("context-wiki",40,610,250,48,word("Artikel suchen","Look up article",
            "Rechercher l’article"),0);
    }
}
static void selection_draw(void) {
    lumen_document_page(0);
    ui_paint_surface(19,515,558,258,16,255,100);
    lumen_wrapped(35,533,526,82,selected_text,PAINT_BODY,3);
    lumen_button_action("selection-copy",35,631,165,48,word("Kopieren","Copy","Copier"),0);
    lumen_button_action("selection-context",217,631,165,48,word("Kontext + Wiki","Context + Wiki","Contexte + Wiki"),0);
    lumen_button_action("selection-dictionary",399,631,160,48,word("Wörterbuch","Dictionary","Dictionnaire"),0);
    lumen_button_action("selection-mark",35,704,250,48,word("Markieren","Highlight","Surligner"),0);
    lumen_button_action("selection-close",310,704,250,48,word("Schließen","Close","Fermer"),0);
}
static void selection_close(void) {
    view=BOOK;chrome=lumen_back=0;
    request("selection_clear","",0);
}
static int selection_action(const char *id) {
    if(!strcmp(id,"selection-context")) {
        selection_close();
        context_open(selected_text,1);
    } else if(!strcmp(id,"selection-dictionary")) {
        selection_close();
        lumen_service("dictionary","lookup",selected_text,6);
    } else if(!strcmp(id,"selection-copy")) {
        view=BOOK;chrome=lumen_back=0;
        request("selection_copy","",4);
    } else if(!strcmp(id,"selection-mark")) {
        view=BOOK;chrome=lumen_back=0;
        request("selection_mark","",5);
    } else if(!strcmp(id,"selection-close"))selection_close();
    else return 0;
    return 1;
}
static void draw(void) {
    if(view==CONTEXT){context_draw();return;}
    if(view==SELECTION){lumen_fullscreen=1;selection_draw();return;}
    lumen_fullscreen=view==BOOK && !chrome && total>0;
    if(view==SETTINGS){draw_settings();return;}
    if(view==CONTENTS) {
        ui_paint_heading(word("Inhalt","Contents","Sommaire"),1);
        if(confirm<0)lumen_icon_action("bookmark",528,74,IC_PIN);
        const char *tabs[]={word("Kapitel","Chapters","Chapitres"),word("Lesezeichen","Bookmarks","Signets"),
            word("Markierungen","Highlights","Surlignages")};
        if(confirm<0)lumen_segments("contents",28,145,544,48,tabs,3,category);
        for(int row=0;row<8 && list_page*8+row<count;row++) {
            int y=213+row*62;
            lumen_wrapped(30,y+5,category==2 ? 480 : 535,49,labels[row],PAINT_BODY,2);
            if(confirm<0) {
                char id[16];snprintf(id,sizeof(id),"entry-%d",row);
                lumen_custom_action(id,28,y,category==2 ? 494 : 544,61);
                if(category==2) {
                    snprintf(id,sizeof(id),"delete-%d",row);
                    lumen_icon_action(id,522,y+8,IC_TRASH);
                }
            }
            lumen_list_divider(28,y+61,572);
        }
        if(!count)ui_paint_text(28,264,544,70,word("Noch keine Einträge.","No entries yet.","Aucune entrée."),PAINT_BODY);
        lumen_pager(745,list_page,(count+7)/8);
        if(confirm>=0) {
            ui_paint_surface(24,275,552,220,16,255,100);
            ui_paint_text(35,295,530,85,word("Markierung löschen?","Delete highlight?","Supprimer le surlignage ?"),PAINT_HEADING);
            lumen_button_action("cancel",38,412,250,52,word("Abbrechen","Cancel","Annuler"),0);
            lumen_button_action("remove",312,412,250,52,word("Löschen","Delete","Supprimer"),1);
        }
        return;
    }
    if(!lumen_ready)return;
    if(!total) {
        ui_paint_heading(word("Lesen","Reader","Lecture"),0);
        lumen_button_action("resume",28,298,544,54,word("Weiterlesen","Continue reading","Reprendre la lecture"),1);
        lumen_button_action("library",28,369,544,54,word("Bibliothek","Library","Bibliothèque"),0);
        return;
    }
    lumen_document_page(chrome);
    if(chrome) {
        ui_paint_surface(0,56,600,77,0,255,-1);
        lumen_text(28,74,420,44,title,PAINT_HEADING,0,1);
        lumen_icon_action("contents",468,74,IC_CONTENTS);
        lumen_icon_action("settings",528,74,IC_SETTINGS);
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
static int index_for(const char *id,const char *prefix) {
    size_t n=strlen(prefix);
    return !strncmp(id,prefix,n) && strlen(id)==n+1 && id[n]>='0' && id[n]<='9' ? id[n]-'0' : -1;
}
static int activate(const char *id) {
    if(view==CONTEXT) {
        if(!strcmp(id,"context-close"))show(BOOK);
        else if(!strcmp(id,"context-passage") && mention_state==2)mention_expanded=!mention_expanded;
        else if(!strcmp(id,"context-wiki") && wiki_state!=1) {
            wiki_state=1;
            lumen_app_service("lumen.wikipedia","summary",context_term,context_action+1);
        }
        return 1;
    }
    if(view==SETTINGS) {
        int index=index_for(id,"setting-");
        if(index>=0) {
            int count=0;const char **options=choices(index,&count);
            if(options)lumen_select(options,count,preferences[index],10+index);
            else if(index==7)setting(index,!preferences[index]);
        } else {
            static const int step[]={0,2,5,3,0,4,2},low[]={0,16,110,0,0,0,0},
                high[]={0,34,190,30,0,24,20};
            int increase=index_for(id,"increase-"),decrease=index_for(id,"decrease-");
            int index=increase>=0 ? increase : decrease;
            if(index>=0 && index<7 && step[index]) {
                int value=preferences[index]+step[index]*(increase>=0 ? 1 : -1);
                if(value<low[index])value=low[index];
                if(value>high[index])value=high[index];
                setting(index,value);
            }
        }
    } else if(view==CONTENTS) {
        if(confirm>=0) {
            if(!strcmp(id,"remove"))number("remove_annotation",confirm,3);
            if(!strcmp(id,"remove") || !strcmp(id,"cancel"))confirm=-1;
        } else if(!strcmp(id,"bookmark"))request("bookmark","",3);
        else {
            int row=index_for(id,"entry-"),remove=index_for(id,"delete-");
            if(category==2 && remove>=0 && remove<8 && list_page*8+remove<count)confirm=positions[remove];
            else if(row>=0 && row<8 && list_page*8+row<count) {
                view=BOOK;chrome=lumen_back=0;
                number(category==2 ? "annotation" : "goto",positions[row],1);
            }
        }
    } else if(!total) {
        if(!strcmp(id,"resume"))request("resume_book","",1);
        else if(!strcmp(id,"library")) {
            if(private_preferences)lumen_open_provider("books");
            else lumen_open("lumen.library");
        }
    } else if(chrome) {
        if(!strcmp(id,"settings")){settings_page=0;show(SETTINGS);}
        else if(!strcmp(id,"contents"))show(CONTENTS);
    }
    return 1;
}
static int input(const Input *in) {
    if(in->kind==INPUT_OPEN) {
        if(lumen_number("api"))private_preferences=!!lumen_number("reader_preferences");
        int values[3];
        if(private_preferences && app_state_load("reader-controls",values,sizeof(values)) && valid_preferences(values)) {
            memcpy(preferences+7,values,sizeof(values));
            preferences_loaded=1;
        }
        lumen_ready=0;state();return 1;
    }
    if(in->kind==INPUT_RESUME || in->kind==INPUT_REFRESH){if(view==CONTENTS)contents();else state();return 1;}
    if(in->kind==INPUT_INTENT) {
        char operation[32];
        if(lumen_string("operation",operation,sizeof(operation)) &&
           (!strcmp(operation,"document-selection") || !strcmp(operation,"selection-context"))) {
            if(!lumen_string("text",selected_text,sizeof(selected_text)) || !*selected_text)return 1;
            if(!strcmp(operation,"selection-context"))context_open(selected_text,lumen_number("lookup"));
            else {view=SELECTION;lumen_back=1;}
            return 1;
        }
        view=BOOK;chrome=0;lumen_back=0;lumen_ready=0;state();return 1;
    }
    if(in->kind==INPUT_BACK && view==SELECTION){selection_close();return 1;}
    if(in->kind==INPUT_BACK && view==CONTEXT && mention_expanded){mention_expanded=0;return 1;}
    if(in->kind==INPUT_BACK && view!=BOOK){if(confirm>=0)confirm=-1;else show(BOOK);return 1;}
    if(in->kind==INPUT_PAGE) {
        if(view==CONTEXT || view==SELECTION)return 1;
        if(view==SETTINGS)settings_page=in->x;
        else if(view==CONTENTS){list_page=in->x;contents();}
        else if(total)number("goto",in->x,1);
        return 1;
    }
    if(in->kind==INPUT_SELECT && view==SETTINGS && in->x>=10 && in->x<20 && in->y>=0)
        setting(in->x-10,in->y);
    if(in->kind==INPUT_SERVICE) {
        if(in->x==0)return 1;
        if(in->x==4 || in->x==5) {
            const char *message=!in->y ? word("Aktion fehlgeschlagen.","Action failed.","Échec de l’action.") :
                in->x==4 ? word("Kopiert.","Copied.","Copié.") :
                            word("Markierung gespeichert.","Highlight saved.","Surlignage enregistré.");
            lumen_request("notify","",message,0);
            return 1;
        }
        if(in->x==6) {
            char definition[2048];
            const char *message=in->y && lumen_string("definition",definition,sizeof(definition)) ? definition :
                word("Kein Eintrag im Offline-Wörterbuch.","No entry in the offline dictionary.","Aucune entrée dans le dictionnaire hors ligne.");
            lumen_request("notify","",message,0);
            return 1;
        }

        if(in->x>=100) {
            if(view!=CONTEXT)return 1;
            if(in->x==context_action) {
                mention_state=3;
                if(in->y && wire_bounded_string("quote",mention_quote,sizeof(mention_quote))) {
                    mention_page=lumen_number("mention_page");
                    mention_offset=lumen_number("match_offset");
                    mention_length=lumen_number("match_length");
                    mention_state=2;
                }
            } else if(in->x==context_action+1) {
                char data[2850];
                wiki_state=3;
                if(in->y && wire_bounded_string("data",data,sizeof(data))) {
                    char *split=strchr(data,'\n');
                    if(split) {
                        *split++=0;
                        snprintf(wiki_title,sizeof(wiki_title),"%s",data);
                        snprintf(wiki_extract,sizeof(wiki_extract),"%s",split);
                        wiki_state=2;
                    }
                }
            }
            return 1;
        }
        if(in->x==1)lumen_ready=1;
        if(in->y) {
            page=lumen_number("page");total=lumen_number("total");
            lumen_string("title",title,sizeof(title));
            int fields=private_preferences && preferences_loaded ? 7 : 10;
            for(int i=0;i<fields;i++){char key[24];snprintf(key,sizeof(key),"setting%d",i);preferences[i]=lumen_number(key);}
            if(private_preferences && !preferences_loaded)import_controls();
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
    if(in->kind==INPUT_COMPONENT) {
        char id[24],gesture[24],value[8];
        if(!lumen_string("id",id,sizeof(id)) || !lumen_string("gesture",gesture,sizeof(gesture)))return 0;
        if(!strcmp(gesture,"clicked"))return view==SELECTION ? selection_action(id) : activate(id);
        if(view==CONTENTS && confirm<0 && !strcmp(id,"contents") && !strcmp(gesture,"selected") &&
           lumen_string("text",value,sizeof(value)) && strlen(value)==1 && value[0]>='0' && value[0]<='2') {
            category=value[0]-'0';list_page=0;contents();
        }
        return 1;
    }
    if((in->kind==INPUT_TAP || in->kind==INPUT_SWIPE) && view==SELECTION){selection_close();return 1;}
    if(in->kind==INPUT_SWIPE && view==BOOK && !chrome && total && preferences[7]){
        number("turn",in->dx<0 ? 1 : -1,1);return 1;
    }
    if(in->kind!=INPUT_TAP || view!=BOOK || !total)return 0;
    if(chrome)chrome=0;
    else if(in->y<78 || in->y>=722)chrome=1;
    else number("turn",in->x<(preferences[8] ? 80 : 200) ? -1 : 1,1);
    return 1;
}
int main(void){lumen_run(draw,input,NULL);return 0;}
