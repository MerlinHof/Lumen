#include "lumen_app.h"

enum {LIST,EDITOR,MOVE};
static int view,folder,parent,page,count,ink,eraser,width,pages,editor_page,delete_pending,selected_kind,selected_id;
static int previous_page, editor_note;
static char folder_title[96];
static struct {int id,kind,children;char title[256],detail[48];} rows[8];
static const char *word(const char *de,const char *en,const char *fr) {
    return lumen_language==1 ? en : lumen_language==2 ? fr : de;
}
static void request(const char *operation,const char *data,int action) {
    char context[256];
    snprintf(context,sizeof(context),"%d\n%s",folder,data);
    lumen_service("notes_editor",operation,context,action);
}
static void back_state(void){lumen_back=folder || view!=LIST || delete_pending;}
#include "collection.h"
static void list(void){collection_list();}
static void collection_changed(int lock,int ok) {
    if(lock>=0)close(lock);
    list();
    if(!ok)lumen_request("notify","",word("Notizen konnten nicht gespeichert werden.",
        "Could not save changes to Notes.","Impossible d’enregistrer les modifications."),0);
}
static void note(const char *operation,int value) {
    char data[24];snprintf(data,sizeof(data),"%d",value);request(operation,data,2);
}
static void show_editor(int id) {
    view=EDITOR;
    editor_note=id;
    editor_page=0;
    back_state();
    note("open",id);
}
static void markdown_preview(void) {
    if(editor_note<=0)return;
    char path[48],text[8192];
    size_t size;
    snprintf(path,sizeof(path),"notes/%d.txt",editor_note);
    if(lumen_storage_read_at(getenv("LUMEN_LEGACY_DATA"),path,text,sizeof(text)-1,&size) &&
       size && lumen_storage_write("preview.md",text,size)) {
        lumen_reader_file(word("Notizen","Notes","Notes"),"preview.md");
    } else lumen_request("notify","",word("Vorschau nicht verfügbar.","Preview unavailable.",
        "Aperçu indisponible."),0);
}
static void draw(void) {
    if(view==EDITOR) {
        lumen_note_surface();
        if(ink) {
            ui_paint_surface(0,56,600,53,0,255,-1);
            lumen_icon_action("ink",11,58,IC_BACK);
            lumen_button_action("eraser",70,58,100,44,eraser ? word("Radierer","Eraser","Gomme") : word("Stift","Pen","Stylo"),eraser);
            lumen_button_action("width",180,58,80,44,width==3 ? word("Fein","Fine","Fin") : word("Breit","Wide","Large"),0);
            lumen_icon_action("undo",280,58,IC_UNDO);
            lumen_icon_action("export",528,58,IC_FULLSCREEN);
        } else {
            lumen_icon_action("ink",470,56,IC_EDIT);
            lumen_icon_action("markdown",532,56,IC_MARKDOWN);
        }
        lumen_pager(748,editor_page,pages);
        return;
    }
    int moving=view==MOVE,per=moving ? 8 : 7;
    ui_paint_heading(moving ? word("Verschieben","Move to folder","Déplacer vers") :
        folder ? folder_title : word("Notizen","Notes","Notes"),3);
    if(!moving && !delete_pending) {
        if(folder)lumen_icon_action("rename",408,74,IC_EDIT);
        lumen_icon_action("new-folder",468,74,IC_FOLDER_PLUS);
        lumen_icon_action("new-note",528,74,IC_NOTE_PLUS);
    }
    for(int i=0;i<per && page*per+i<count;i++) {
        int height=moving ? 66 : 78,y=151+i*height;
        ui_paint_icon_button(30,y+(height-44)/2,rows[i].kind ? IC_FOLDER : IC_NOTE);
        lumen_text(84,y+7,424,36,rows[i].title,PAINT_BODY,0,1);
        char subtitle[48];snprintf(subtitle,sizeof(subtitle),"%d %s",rows[i].children,word("Einträge","items","éléments"));
        lumen_text(84,y+41,424,25,rows[i].kind ? subtitle : rows[i].detail,PAINT_SECONDARY,-1,1);
        if(!delete_pending) {
            char action[16];snprintf(action,sizeof(action),"entry-%d",i);
            lumen_custom_action(action,28,y,moving ? 544 : 494,height);
            if(!moving) {
                snprintf(action,sizeof(action),"menu-%d",i);
                lumen_icon_action(action,522,y+17,IC_MORE);
            }
        }
        ui_paint_line(28,y+height-1,572,y+height-1,1,190);
    }
    if(!count)lumen_text(30,161,540,49,word("Dieser Ordner ist leer.","This folder is empty.","Ce dossier est vide."),PAINT_SECONDARY,-1,1);
    if(delete_pending) {
        ui_paint_surface(24,275,552,220,16,255,100);
        ui_paint_text(35,295,530,85,selected_kind ?
            word("Ordner und Inhalt löschen?","Delete folder and contents?","Supprimer le dossier et son contenu ?") :
            word("Notiz löschen?","Delete note?","Supprimer la note ?"),PAINT_HEADING);
        lumen_button_action("cancel",38,412,250,52,word("Abbrechen","Cancel","Annuler"),0);
        lumen_button_action("confirm",312,412,250,52,word("Löschen","Delete","Supprimer"),1);
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
        if(wire_bounded_string("text",value,sizeof(value)) && *value) {
            const char *root=getenv("LUMEN_LEGACY_DATA");
            int lock=collection_lock(root);
            collection_changed(lock,lock>=0 && collection_edit_folder(root,value,in->x==2));
        }
        return 1;
    }
    if(in->kind==INPUT_SERVICE) {
        if(in->x==2 && in->y) {
            editor_page=lumen_number("page");pages=lumen_number("pages");
            ink=lumen_number("ink");eraser=lumen_number("eraser");width=lumen_number("width");
            int saved=lumen_number("note_id");
            if(saved>0)editor_note=saved;
        } else if(in->x==2 && view==EDITOR && !in->y) {
            view=LIST;collection_changed(-1,0);
        } else if(in->x==3)list();
        back_state();return 1;
    }
    if(in->kind==INPUT_TAP && view==EDITOR && !ink && in->y>=56 && in->y<742) {
        /* The editor maps this point to a text caret. */
        char value[32];snprintf(value,sizeof(value),"%d %d",in->x,in->y);
        request("edit",value,2);
        return 1;
    }
    char id[24],gesture[24];
    if(in->kind!=INPUT_COMPONENT || !lumen_string("id",id,sizeof(id)) ||
       !lumen_string("gesture",gesture,sizeof(gesture)) || strcmp(gesture,"clicked"))return 0;
    if(view==EDITOR) {
        if(!ink && !strcmp(id,"markdown"))markdown_preview();
        else if(!strcmp(id,"ink") ||
           (ink && (!strcmp(id,"eraser") || !strcmp(id,"width") || !strcmp(id,"undo") || !strcmp(id,"export"))))
            request(id,"",2);
        return 1;
    }
    if(delete_pending) {
        if(!strcmp(id,"confirm")) {
            const char *root=getenv("LUMEN_LEGACY_DATA");
            int lock=collection_lock(root);
            collection_changed(lock,lock>=0 && collection_delete(root,selected_kind,selected_id));
        }
        if(!strcmp(id,"confirm") || !strcmp(id,"cancel")){delete_pending=0;back_state();}
        return 1;
    }
    if(view==LIST) {
        if(!strcmp(id,"new-note")){show_editor(0);return 1;}
        if(!strcmp(id,"new-folder")){lumen_request("input",word("Neuer Ordner","New folder","Nouveau dossier"),"",1);return 1;}
        if(!strcmp(id,"rename") && folder){lumen_request("input",word("Ordner umbenennen","Rename folder","Renommer le dossier"),folder_title,2);return 1;}
    }
    int menu=!strncmp(id,"menu-",5),per=view==MOVE ? 8 : 7;
    if(!menu && strncmp(id,"entry-",6))return 0;
    const char *digit=id+(menu ? 5 : 6);
    if(strlen(digit)!=1 || *digit<'0' || *digit>='0'+per)return 0;
    int row=*digit-'0';
    if(page*per+row>=count)return 0;
    if(view==MOVE && !menu) {
        int destination=rows[row].id;
        view=LIST;page=previous_page;
        const char *root=getenv("LUMEN_LEGACY_DATA");
        int lock=collection_lock(root);
        collection_changed(lock,lock>=0 && collection_move(root,selected_kind,selected_id,destination));
        back_state();
    } else if(menu) {
        selected_kind=rows[row].kind;selected_id=rows[row].id;
        const char *choices[]={word("Verschieben","Move","Déplacer"),word("Löschen","Delete","Supprimer")};
        lumen_select(choices,2,-1,1);
    } else if(rows[row].kind){folder=rows[row].id;page=0;list();back_state();}
    else show_editor(rows[row].id);
    return 1;
}
int main(void){lumen_service_handler=serve;lumen_run(draw,input,NULL);return 0;}
