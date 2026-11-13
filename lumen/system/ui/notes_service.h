/* Shared note collection and ink surface used by Notes, Study and export. */
static int notes_has_id(int id) {
    for(int i=0;i<note_count;i++)if(note_ids[i]==id)return 1;
    return 0;
}
static int notes_service_reply(int action) {
    char *message=NULL;size_t length=0;
    FILE *out=package_reply(&message,&length,action,1);
    if(!out)return 0;
    fprintf(out,",\"page\":%d,\"pages\":%d,\"ink\":%d,\"eraser\":%d,\"width\":%d",
        extra_page,u15_note_page_count(),note_ink_mode,ink_eraser,ink_width);
    package_reply_end(out,&message);return 1;
}
static int package_notes(const char *operation,const char *data,int action) {
    if(app_for_route(ui_route())!=APP_NOTES)return 0;
    u16_notes_defaults();
    char *context_end;
    long folder=strtol(data,&context_end,10);
    if(context_end==data || *context_end!='\n' || folder<0 || folder>INT_MAX ||
       (folder && folder15_index((int)folder)<0))return package_acknowledge(action,0);
    note_tree.current=(int)folder;
    data=context_end+1;
    if(!strcmp(operation,"open") || !strcmp(operation,"delete") || !strcmp(operation,"move"))
        notes_scan();
    if(!strcmp(operation,"new_folder") || !strcmp(operation,"rename_folder")) {
        if(!*data || strlen(data)>=96)return package_acknowledge(action,0);
        int index=folder15_index(note_tree.current);
        if(!strcmp(operation,"new_folder")) {
            if(note_tree.nf>=512)return package_acknowledge(action,0);
            int id=1;for(int i=0;i<note_tree.nf;i++)if(note_tree.folder[i].id>=id)id=note_tree.folder[i].id+1;
            index=note_tree.nf++;note_tree.folder[index]=(NoteFolder){.id=id,.parent=note_tree.current};
        }
        if(index<0)return package_acknowledge(action,0);
        snprintf(note_tree.folder[index].name,96,"%s",data);v1_dirty=1;v1_save();
        return package_acknowledge(action,1);
    }
    if(!strcmp(operation,"delete") || !strcmp(operation,"move")) {
        int kind,id,destination=0;char tail;
        int moving=!strcmp(operation,"move");
        int fields=moving ? sscanf(data,"%d %d %d %c",&kind,&id,&destination,&tail) :
            sscanf(data,"%d %d %c",&kind,&id,&tail);
        if(fields!=(moving ? 3 : 2) || (kind!=0 && kind!=1) || id<=0 ||
           (kind ? folder15_index(id)<0 : !notes_has_id(id)))return package_acknowledge(action,0);
        if(moving) {
            if(destination && (folder15_index(destination)<0 ||
               (kind && folder15_inside(destination,id))))return package_acknowledge(action,0);
            if(kind){note_tree.folder[folder15_index(id)].parent=destination;v1_dirty=1;}
            else v1_place(id,destination);
        } else if(!kind)note_delete16(id);
        else {
            for(int i=0;i<note_count;i++)if(folder15_inside(v1_folder_for(note_ids[i]),id))note_delete16(note_ids[i]);
            int remove[512]={0};
            for(int i=0;i<note_tree.nf;i++)remove[i]=folder15_inside(note_tree.folder[i].id,id);
            for(int i=note_tree.nf-1;i>=0;i--)if(remove[i]) {
                memmove(note_tree.folder+i,note_tree.folder+i+1,(size_t)(--note_tree.nf-i)*sizeof(NoteFolder));
                v1_dirty=1;
            }
        }
        v1_save();notes_scan();return package_acknowledge(action,1);
    }
    if(!strcmp(operation,"open")) {
        char *end;long id=strtol(data,&end,10);
        if(!*data || *end || id<0 || id>INT_MAX || (id && !notes_has_id((int)id)))
            return package_acknowledge(action,0);
        ink_save();note_ink_mode=0;extra_page=0;
        if(id)note_load((int)id);else{note_selected=-1;note_text[0]=0;}
        editor_ui.note_view_start=0;
        app_runtime_route(&runtime,30);
        if(!id)open_keyboard(30,"Notiz","");
        return notes_service_reply(action);
    }
    if(!strcmp(operation,"close")) {
        ink_save();note_ink_mode=0;
        app_runtime_route(&runtime,29);
        return package_acknowledge(action,1);
    }
    if(ui_route()!=30)return package_acknowledge(action,0);
    if(!strcmp(operation,"edit")) {
        int x,y;char tail;
        if(sscanf(data,"%d %d %c",&x,&y,&tail)!=2 || y<54 || y>=742)return package_acknowledge(action,0);
        open_keyboard(30,"Notiz",note_text);
        editor_ui.note_cursor=strlen(editor_ui.edit);editor_ui.note_view_start=extra_page*23;
        u15_note_editor_touch(x,y);
    } else if(!strcmp(operation,"markdown")) {
        reader_markdown17("Notizen",note_text,30);return 1;
    } else if(!strcmp(operation,"page")) {
        int next=atoi(data);
        if(next<0 || next>=u15_note_page_count())return package_acknowledge(action,0);
        ink_save();extra_page=next;editor_ui.note_view_start=23*extra_page;
    } else if(!strcmp(operation,"ink")) {
        if(!note_ink_mode) {
            if(note_selected<0)extra_commit(30,*note_text ? note_text : "Notiz");
            ink_load();note_ink_mode=1;
        } else {ink_save();note_ink_mode=0;}
    } else if(!strcmp(operation,"eraser"))ink_eraser^=1;
    else if(!strcmp(operation,"width"))ink_width=ink_width==3 ? 7 : 3;
    else if(!strcmp(operation,"undo") && note_ink_mode) {
        memcpy(ink_pixels,ink_undo,sizeof(ink_pixels));ink_dirty=1;
    } else if(!strcmp(operation,"export") && note_ink_mode)u15_export_photo();
    else if(strcmp(operation,"state"))return package_acknowledge(action,0);
    return notes_service_reply(action);
}
