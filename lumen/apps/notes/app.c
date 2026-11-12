#include "lumen_app.h"

enum {LIST,EDITOR,MOVE};
static int view,folder,parent,page,count,ink,eraser,width,pages,editor_page,delete_pending,selected_kind,selected_id;
static int previous_page;
static char folder_title[96];
static struct {int id,kind,children;char title[256],detail[48];} rows[8];
static const char *word(const char *de,const char *en,const char *fr) {
    return lumen_language==1 ? en : lumen_language==2 ? fr : de;
}
static void request(const char *operation,const char *data,int action){lumen_service("notes",operation,data,action);}
static void back_state(void){lumen_back=folder || view!=LIST || delete_pending;}
static void list(void) {
    char value[36];snprintf(value,sizeof(value),"%d %d",folder,page);
    request(view==MOVE ? "folders" : "list",value,1);
}
static void note(const char *operation,int value) {
    char data[24];snprintf(data,sizeof(data),"%d",value);request(operation,data,2);
}
static void show_editor(int id){view=EDITOR;editor_page=0;back_state();note("open",id);}
static void draw(void) {
    if(view==EDITOR) {
        lumen_note_surface();
        if(ink) {
            ui_paint_surface(0,56,600,53,0,255,-1);
            ui_paint_icon_button(11,58,IC_BACK);
            lumen_button(70,58,100,44,eraser ? word("Radierer","Eraser","Gomme") : word("Stift","Pen","Stylo"),eraser);
            lumen_button(180,58,80,44,width==3 ? word("Fein","Fine","Fin") : word("Breit","Wide","Large"),0);
            ui_paint_icon_button(280,58,IC_UNDO);
            ui_paint_icon_button(528,58,IC_FULLSCREEN);
        } else {
            ui_paint_icon_button(470,56,IC_EDIT);
            ui_paint_icon_button(532,56,IC_MARKDOWN);
        }
        lumen_pager(748,editor_page,pages);
        return;
    }
    int moving=view==MOVE,per=moving ? 8 : 7;
    ui_paint_heading(moving ? word("Verschieben","Move to folder","Déplacer vers") :
        folder ? folder_title : word("Notizen","Notes","Notes"),3);
    if(!moving) {
        if(folder)ui_paint_icon_button(408,74,IC_EDIT);
        ui_paint_icon_button(468,74,IC_FOLDER_PLUS);
        ui_paint_icon_button(528,74,IC_NOTE_PLUS);
    }
    for(int i=0;i<per && page*per+i<count;i++) {
        int height=moving ? 66 : 78,y=151+i*height;
        ui_paint_icon_button(30,y+(height-44)/2,rows[i].kind ? IC_FOLDER : IC_NOTE);
        lumen_text(84,y+7,424,36,rows[i].title,PAINT_BODY,0,1);
        char subtitle[48];snprintf(subtitle,sizeof(subtitle),"%d %s",rows[i].children,word("Einträge","items","éléments"));
        lumen_text(84,y+41,424,25,rows[i].kind ? subtitle : rows[i].detail,PAINT_SECONDARY,-1,1);
        if(!moving)ui_paint_icon_button(522,y+17,IC_MORE);
        ui_paint_hitbox(28,y,544,height);
        ui_paint_line(28,y+height-1,572,y+height-1,1,190);
    }
    if(!count)lumen_text(30,161,540,49,word("Dieser Ordner ist leer.","This folder is empty.","Ce dossier est vide."),PAINT_SECONDARY,-1,1);
    if(delete_pending) {
        ui_paint_surface(24,275,552,220,16,255,100);
        ui_paint_text(35,295,530,85,selected_kind ?
            word("Ordner und Inhalt löschen?","Delete folder and contents?","Supprimer le dossier et son contenu ?") :
            word("Notiz löschen?","Delete note?","Supprimer la note ?"),PAINT_HEADING);
        lumen_button(38,412,250,52,word("Abbrechen","Cancel","Annuler"),0);
        lumen_button(312,412,250,52,word("Löschen","Delete","Supprimer"),1);
    } else lumen_pager(746,page,(count+per-1)/per);
}
static int input(const Input *in) {
    if(in->kind==INPUT_OPEN || in->kind==INPUT_RESUME || in->kind==INPUT_REFRESH){
        if(view==EDITOR)request("state","",2);else list();
        return 1;
    }
    if(in->kind==INPUT_INTENT) {
        char operation[32];lumen_string("operation",operation,sizeof(operation));
        int id=lumen_number("note_id");
        if(!strcmp(operation,"note-open") && id>0)show_editor(id);
        return 1;
    }
    if(in->kind==INPUT_BACK) {
        if(delete_pending)delete_pending=0;
        else if(view==EDITOR) {
            if(ink){request("ink","",2);return 1;}
            view=LIST;request("close","",3);
        } else if(view==MOVE){view=LIST;page=previous_page;list();}
        else if(folder){folder=parent;page=0;list();}
        back_state();return 1;
    }
    if(in->kind==INPUT_PAGE) {
        if(view==EDITOR)note("page",in->x);
        else {page=in->x;list();}
        return 1;
    }
    if(in->kind==INPUT_SELECT && in->x==1 && in->y>=0) {
        if(in->y==0){previous_page=page;view=MOVE;page=0;list();}
        else if(in->y==1)delete_pending=1;
        back_state();return 1;
    }
    if(in->kind==INPUT_TEXT && (in->x==1 || in->x==2)) {
        char value[96];
        if(lumen_string("text",value,sizeof(value)) && *value)
            request(in->x==1 ? "new_folder" : "rename_folder",value,3);
        return 1;
    }
    if(in->kind==INPUT_SERVICE) {
        if(in->x==1 && in->y) {
            parent=lumen_number("parent");folder=lumen_number("folder_id");
            page=lumen_number("page");count=lumen_number("count");
            lumen_string("folder",folder_title,sizeof(folder_title));
            for(int i=0;i<8;i++) {
                char key[24];
                snprintf(key,sizeof(key),"id%d",i);rows[i].id=lumen_number(key);
                snprintf(key,sizeof(key),"kind%d",i);rows[i].kind=lumen_number(key);
                snprintf(key,sizeof(key),"children%d",i);rows[i].children=lumen_number(key);
                snprintf(key,sizeof(key),"title%d",i);lumen_string(key,rows[i].title,sizeof(rows[i].title));
                snprintf(key,sizeof(key),"detail%d",i);lumen_string(key,rows[i].detail,sizeof(rows[i].detail));
            }
        } else if(in->x==2 && in->y) {
            editor_page=lumen_number("page");pages=lumen_number("pages");
            ink=lumen_number("ink");eraser=lumen_number("eraser");width=lumen_number("width");
        } else if(in->x==3)list();
        back_state();return 1;
    }
    if(in->kind!=INPUT_TAP)return 0;
    int x=in->x,y=in->y;
    if(view==EDITOR) {
        if(y>=56 && y<106) {
            if(ink){
                if(x<65)request("ink","",2);
                else if(x<175)request("eraser","",2);
                else if(x<270)request("width","",2);
                else if(x<340)request("undo","",2);
                else if(x>=500)request("export","",2);
            } else if(x>=532)request("markdown","",2);
            else if(x>=470)request("ink","",2);
            else goto edit;
        } else if(!ink && y>=56 && y<742) {
edit:       ;
            char value[32];snprintf(value,sizeof(value),"%d %d",x,y);request("edit",value,2);
        }
        return 1;
    }
    if(delete_pending) {
        if(y>=412 && y<464){
            if(x>=312){
                char value[48];snprintf(value,sizeof(value),"%d %d",selected_kind,selected_id);
                request("delete",value,3);
            }
            delete_pending=0;back_state();
        }
        return 1;
    }
    if(view==LIST && y>=74 && y<118) {
        if(x>=528)show_editor(0);
        else if(x>=468)lumen_request("input",word("Neuer Ordner","New folder","Nouveau dossier"),"",1);
        else if(x>=408 && folder)lumen_request("input",word("Ordner umbenennen","Rename folder","Renommer le dossier"),folder_title,2);
        return 1;
    }
    int per=view==MOVE ? 8 : 7,height=view==MOVE ? 66 : 78;
    if(y>=151 && y<151+per*height) {
        int r=(y-151)/height;
        if(page*per+r<count) {
            if(view==MOVE) {
                char value[64];snprintf(value,sizeof(value),"%d %d %d",selected_kind,selected_id,rows[r].id);
                view=LIST;page=previous_page;request("move",value,3);back_state();
            } else if(x>=510) {
                selected_kind=rows[r].kind;selected_id=rows[r].id;
                const char *choices[]={word("Verschieben","Move","Déplacer"),word("Löschen","Delete","Supprimer")};
                lumen_select(choices,2,-1,1);
            } else if(rows[r].kind){folder=rows[r].id;page=0;list();back_state();}
            else show_editor(rows[r].id);
        }
    }
    return 1;
}
int main(void){lumen_run(draw,input,NULL);return 0;}
