/* Protocol 1 bridge for the existing paginated text and ink editor. */
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
    char *context_end;
    long folder=strtol(data,&context_end,10);
    if(context_end==data || *context_end!='\n' || folder<0 || folder>INT_MAX)
        return package_acknowledge(action,0);
    data=context_end+1;
    if(!strcmp(operation,"open")) {
        char *end;long id=strtol(data,&end,10);
        if(!*data || *end || id<0 || id>INT_MAX || (id && !note_load((int)id)))
            return package_acknowledge(action,0);
        ink_save();note_ink_mode=0;extra_page=0;
        note_editor_folder=(int)folder;
        if(!id){note_selected=-1;note_text[0]=0;}
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
        char *end;
        long next=strtol(data,&end,10);
        if(end==data || *end || next<0 || next>=u15_note_page_count())return package_acknowledge(action,0);
        ink_save();extra_page=next;editor_ui.note_view_start=23*extra_page;
    } else if(!strcmp(operation,"ink")) {
        if(!note_ink_mode) {
            if(note_selected<0)extra_commit(30,*note_text ? note_text : "Notiz");
            if(note_selected<0)return package_acknowledge(action,0);
            ink_load();note_ink_mode=1;
        } else {ink_save();note_ink_mode=0;}
    } else if(!strcmp(operation,"eraser"))ink_eraser^=1;
    else if(!strcmp(operation,"width"))ink_width=ink_width==3 ? 7 : 3;
    else if(!strcmp(operation,"undo") && note_ink_mode) {
        memcpy(ink_pixels,ink_undo,sizeof(ink_pixels));ink_dirty=1;
        ink_save_at=extra_millis()+1500;
    } else if(!strcmp(operation,"export") && note_ink_mode)u15_export_photo();
    else if(strcmp(operation,"state"))return package_acknowledge(action,0);
    return notes_service_reply(action);
}
