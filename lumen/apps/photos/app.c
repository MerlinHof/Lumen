#include "lumen_app.h"

enum {GALLERY,PHOTO,SETTINGS};
static char ids[16][17],albums[17][64];
static int memberships[16],menu_ids[17];
static int page,count,folder,view,selected,confirm,fullscreen,slideshow,fill,rotation,seconds=10;
static int photo_fill,photo_controls;
static const char *word(const char *de,const char *en,const char *fr) {
    return lumen_language==1 ? en : lumen_language==2 ? fr : de;
}
static void load(void) {
    char value[32];snprintf(value,sizeof(value),"%d %d",folder,page);
    lumen_service("photos","list",value,1);
}
static void request(const char *operation,const char *data,int action) {
    lumen_service("photos",operation,data,action);
}
static void open_gallery(void) {
    view=GALLERY;fullscreen=slideshow=photo_fill=confirm=lumen_back=0;load();
}
static void menu(int assigning) {
    const char *labels[17];char choices[17][80];
    int n=0,chosen=0;
    if(!assigning){labels[n]=word("Alle Fotos","All photos","Toutes les photos");menu_ids[n++]=0;}
    for(int i=1;i<=16;i++)if(*albums[i]) {
        if(i==folder)chosen=n;
        snprintf(choices[n],sizeof(choices[n]),"%s%s",assigning && (memberships[selected]&(1<<(i-1))) ? "✓ " : "",albums[i]);
        labels[n]=choices[n];menu_ids[n++]=i;
    }
    if(n)lumen_select(labels,n,assigning ? -1 : chosen,assigning ? 2 : 1);
}
static void show_photo(int i,int show) {
    if(i<0 || i>=16 || !*ids[i])return;
    view=PHOTO;selected=i;photo_fill=0;photo_controls=1;
    slideshow=show;lumen_back=1;
}
static void advance(int delta) {
    if(count<1)return;
    int index=page*16+selected+delta;
    if(slideshow)index=(index+count)%count;
    if(index<0 || index>=count)return;
    selected=index%16;
    if(index/16!=page){page=index/16;ids[selected][0]=0;load();}
}
static void draw(void) {
    fullscreen=view==PHOTO && (slideshow || (photo_fill && !photo_controls));
    lumen_fullscreen=fullscreen;
    if(view==PHOTO) {
        if(*ids[selected])lumen_photo(0,fullscreen ? 0 : 56,600,fullscreen ? 800 : 680,ids[selected],
            slideshow ? fill : photo_fill,slideshow ? rotation : 0,fullscreen);
        if(slideshow)lumen_schedule(seconds*1000);
        if(!fullscreen) {
            const int icon_ids[]={IC_PREV,IC_ROTATE_PHOTO,IC_FOLDER,IC_TRASH,IC_FULLSCREEN,IC_NEXT};
            const int left[]={28,148,216,278,398,528};
            for(int i=0;i<6;i++)ui_paint_icon_button(left[i],748,icon_ids[i]);
        }
    } else {
        ui_paint_heading(word("Fotos","Photos","Photos"),3);
        ui_paint_icon_button(408,74,IC_FOLDER_PLUS);ui_paint_icon_button(468,74,IC_CONNECT);
        ui_paint_icon_button(528,74,view==SETTINGS ? IC_CLOSE : IC_SETTINGS);
        if(view==SETTINGS) {
            char duration[16];snprintf(duration,sizeof(duration),"%d s",seconds);
            lumen_select_row(203,word("Dauer pro Foto","Duration per photo","Durée par photo"),duration);
            lumen_select_row(271,word("Bildformat","Image fit","Ajustement"),fill ? word("Ausfüllen","Fill","Remplir") : word("Einpassen","Fit","Ajuster"));
            lumen_select_row(339,word("Bildausrichtung","Orientation","Orientation"),rotation ? word("Querformat","Landscape","Paysage") : word("Hochformat","Portrait","Portrait"));
        } else {
            lumen_button(28,133,328,44,folder ? albums[folder] : word("Alle Fotos","All photos","Toutes les photos"),0);
            if(count)lumen_button(376,133,196,44,word("Diashow","Slideshow","Diaporama"),1);
            for(int i=0;i<16 && *ids[i];i++) {
                int x=19+i%4*142,y=187+i/4*139;
                lumen_photo(x,y,136,136,ids[i],1,0,0);
                ui_paint_hitbox(x,y,136,136);
            }
            if(!count)lumen_wrapped(30,215,540,130,word("Hier erscheinen deine Fotos.",
                "Your photos will appear here.","Vos photos apparaîtront ici."),PAINT_BODY,4);
            lumen_pager(746,page,(count+15)/16);
        }
    }
    if(confirm) {
        ui_paint_surface(24,275,552,220,16,255,100);
        ui_paint_text(35,295,530,85,word("Foto löschen?","Delete photo?","Supprimer la photo ?"),PAINT_HEADING);
        lumen_button(38,412,250,52,word("Abbrechen","Cancel","Annuler"),0);
        lumen_button(312,412,250,52,word("Löschen","Delete","Supprimer"),1);
    }
}
static int input(const Input *in) {
    static const int durations[]={5,10,15,20,30,60,120};
    if(in->kind==INPUT_OPEN || in->kind==INPUT_RESUME || in->kind==INPUT_REFRESH){load();return 1;}
    if(in->kind==INPUT_BACK) {
        if(confirm)confirm=0;
        else if(view!=GALLERY)open_gallery();
        return 1;
    }
    if(in->kind==INPUT_PAGE && view==GALLERY){page=in->x;load();return 1;}
    if(in->kind==INPUT_TICK && slideshow){advance(1);return 1;}
    if(in->kind==INPUT_SWIPE && view==PHOTO && !slideshow){advance(in->dx<0 ? 1 : -1);return 1;}
    if(in->kind==INPUT_TEXT && in->x==1) {
        char value[64];if(lumen_string("text",value,sizeof(value)) && *value)request("album",value,3);
        return 1;
    }
    if(in->kind==INPUT_SELECT && in->y>=0) {
        int option=in->y;
        if(in->x==1 && option<17){folder=menu_ids[option];page=0;load();}
        if(in->x==2 && option<17){
            char data[24];snprintf(data,sizeof(data),"%s %d",ids[selected],menu_ids[option]);
            request("assign",data,2);
            if(menu_ids[option]==folder && (memberships[selected]&(1<<(folder-1))))view=GALLERY;
        }
        if(in->x==3 && option<2){if(!option)menu(1);else request("lock",ids[selected],2);}
        if(in->x>=4 && in->x<=6) {
            char value[16];
            if(in->x==4 && option<7){snprintf(value,sizeof(value),"%d",durations[option]);request("duration",value,2);}
            if(in->x>4 && option<2){snprintf(value,sizeof(value),"%d",option);request(in->x==5 ? "fill" : "landscape",value,2);}
        }
        return 1;
    }
    if(in->kind==INPUT_SERVICE) {
        if(in->x==1 && in->y) {
            folder=lumen_number("folder");page=lumen_number("page");count=lumen_number("count");
            fill=lumen_number("fill");rotation=lumen_number("landscape");seconds=lumen_number("duration");
            if(seconds<5 || seconds>120)seconds=10;
            for(int i=1;i<=16;i++) {
                char key[24];snprintf(key,sizeof(key),"folder%d",i);lumen_string(key,albums[i],sizeof(albums[i]));
            }
            for(int i=0;i<16;i++){
                char key[24];snprintf(key,sizeof(key),"id%d",i);lumen_string(key,ids[i],sizeof(ids[i]));
                snprintf(key,sizeof(key),"albums%d",i);memberships[i]=lumen_number(key);
            }
        } else if(in->x==3 && in->y){folder=lumen_number("folder");page=0;load();}
        else if(in->x==2)load();
        return 1;
    }
    if(in->kind!=INPUT_TAP && in->kind!=INPUT_HOLD)return 0;
    int x=in->x,y=in->y;
    if(confirm) {
        if(y>=412 && y<464 && in->kind==INPUT_TAP) {
            if(x>=312){request("delete",ids[selected],2);view=GALLERY;lumen_back=0;}
            confirm=0;
        }
        return 1;
    }
    if(view==PHOTO) {
        if(slideshow)open_gallery();
        else if(fullscreen){photo_fill=0;photo_controls=1;}
        else if(y>=748) {
            if(x<85)advance(-1);
            else if(x>=528)advance(1);
            else if(x>=398 && x<444){photo_fill=1;photo_controls=0;}
            else if(x>=278 && x<325)confirm=1;
            else if(x>=216 && x<263)menu(1);
            else if(x>=148 && x<194)request("rotate",ids[selected],2);
        }
        return 1;
    }
    if(y>=74 && y<118 && in->kind==INPUT_TAP) {
        if(x>=528){view=view==SETTINGS ? GALLERY : SETTINGS;lumen_back=view!=GALLERY;}
        else if(x>=468)lumen_open("lumen.transfer");
        else if(x>=408)lumen_request("input",word("Neuer Ordner","New album","Nouvel album"),"",1);
        return 1;
    }
    if(view==SETTINGS && x>=360) {
        if(y>=203 && y<251){
            const char *choices[]={"5 s","10 s","15 s","20 s","30 s","60 s","120 s"};
            int current=1;for(int i=0;i<7;i++)if(durations[i]==seconds)current=i;
            lumen_select(choices,7,current,4);
        } else if(y>=271 && y<319) {
            const char *choices[]={word("Einpassen","Fit","Ajuster"),word("Ausfüllen","Fill","Remplir")};
            lumen_select(choices,2,fill,5);
        } else if(y>=339 && y<387) {
            const char *choices[]={word("Hochformat","Portrait","Portrait"),word("Querformat","Landscape","Paysage")};
            lumen_select(choices,2,rotation,6);
        }
    } else if(view==GALLERY) {
        if(y>=133 && y<177){if(x<356)menu(0);else show_photo(0,1);}
        else if(x>=19 && x<581 && y>=187 && y<740) {
            int i=(y-187)/139*4+(x-19)/142;
            if(i>=0 && i<16 && *ids[i]){
                if(in->kind==INPUT_TAP)show_photo(i,0);
                else {
                    selected=i;
                    const char *choices[]={word("Ordner","Album","Album"),word("Als Sperrbild verwenden","Use as lock screen","Utiliser en veille")};
                    lumen_select(choices,2,-1,3);
                }
            }
        }
    }
    return 1;
}
int main(void){lumen_run(draw,input,NULL);return 0;}
