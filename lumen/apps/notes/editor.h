/* Notes owns the document and page state. The shell measures glyphs and
 * handles the shared text input overlay. */
enum { NOTE_ROWS=23, NOTE_TEXT_ACTION=4, NOTE_LAYOUT_ACTION=5 };
static char editor_text[8192];
static int editor_offsets[8193],editor_numbers[8192],editor_rows,layout_pending;

static void editor_measure(void) {
    pages=1;editor_rows=1;editor_offsets[0]=editor_offsets[1]=0;editor_numbers[0]=1;
    layout_pending=!!*editor_text;
    if(layout_pending)lumen_text_measure(editor_text,528,420,2,NOTE_ROWS,NOTE_LAYOUT_ACTION);
}
static void editor_layout(void) {
    char lengths[32768];
    if(!layout_pending || !wire_bounded_string("breaks",lengths,sizeof(lengths)))return;
    int number=1,rows=0;
    size_t size=strlen(editor_text),offset=0;
    for(char *at=lengths;offset<size;) {
        char *end;
        long count=strtol(at,&end,10);
        if(end==at || count<1 || (size_t)count>size-offset || rows>=8191)return;
        editor_offsets[rows]=offset;
        editor_numbers[rows++]=number;
        offset+=(size_t)count;
        if(editor_text[offset-1]=='\n')number++;
        at=end;
    }
    if(size && editor_text[size-1]=='\n'){
        editor_offsets[rows]=size;
        editor_numbers[rows++]=number;
    }
    editor_offsets[rows]=size;
    editor_rows=rows;
    pages=(editor_rows+NOTE_ROWS-1)/NOTE_ROWS;
    layout_pending=0;
}
static int editor_save(const char *text) {
    const char *root=getenv("LUMEN_LEGACY_DATA");
    int lock=collection_lock(root);
    if(lock<0)return 0;
    int ok;
    if(editor_note<=0) {
        editor_note=collection_create_note(root,folder,text);
        ok=editor_note>0;
    } else {
        char path[64];snprintf(path,sizeof(path),"notes/%d.txt",editor_note);
        ok=lumen_storage_write_at(root,path,text,strlen(text));
    }
    close(lock);
    return ok;
}
static void editor_error(void) {
    lumen_request("notify","",word("Notiz konnte nicht gespeichert werden.",
        "Could not save note.","Impossible d’enregistrer la note."),0);
}
static void editor_keyboard(int cursor) {
    lumen_multiline_at(word("Notiz","Note","Note"),editor_text,NOTE_TEXT_ACTION,cursor,1,1);
}
static void show_editor(int id) {
    if(view==EDITOR && !ink_save()){editor_error();return;}
    size_t size=0;
    char path[64],text[sizeof(editor_text)];
    snprintf(path,sizeof(path),"notes/%d.txt",id);
    if(id && !lumen_storage_read_at(getenv("LUMEN_LEGACY_DATA"),path,text,sizeof(text)-1,&size)){editor_error();return;}
    text[size]=0;
    memcpy(editor_text,text,size+1);
    view=EDITOR;editor_note=id;editor_page=0;ink=eraser=0;width=3;
    ink_load();editor_measure();back_state();
    if(!id)editor_keyboard(0);
}
static void editor_draw(void) {
    int first=editor_page*NOTE_ROWS;
    for(int r=0;r<NOTE_ROWS;r++) {
        int i=first+r,y=56+r*29,w=r<2 ? 420 : 528;
        ui_paint_line(48,y+22,571,y+22,1,231);
        if(i>=editor_rows || layout_pending) {
            if(!ink)lumen_editor_action("end",48,y,w,29,"",-1);
            continue;
        }
        char text[1024],number[16],id[24];
        int count=editor_offsets[i+1]-editor_offsets[i];
        if(count && editor_text[editor_offsets[i]+count-1]=='\n')count--;
        snprintf(text,sizeof(text),"%.*s",count,editor_text+editor_offsets[i]);
        if(!i || editor_numbers[i]!=editor_numbers[i-1]) {
            snprintf(number,sizeof(number),"%d",editor_numbers[i]);
            lumen_text(0,y,45,26,number,PAINT_SECONDARY,135,0);
        }
        if(ink)lumen_text(48,y,w,28,text,PAINT_BODY,0,1);
        else {
            snprintf(id,sizeof(id),"line-%d",i);
            lumen_editor_action(id,48,y,w,29,text,-1);
        }
    }
    if(ink_present)lumen_bitmap("ink-surface.raw",0,56,600,INK_HEIGHT-2,ink_revision);
}
static void export_image(void) {
    if(!ink_save()){editor_error();return;}
    lumen_capture_begin("page.pgm",0,56,600,686,6);
    editor_draw();
    lumen_capture_end();
}
static int save_export_image(void) {
    static unsigned char photo[480064],raster[600*686+32];
    const char *root=getenv("LUMEN_LEGACY_DATA");
    if(!root || !*root)return 0;
    size_t size;char path[96];
    if(!lumen_storage_read("page.pgm",raster,sizeof(raster),&size))return 0;
    const char header[]="P5\n600 686\n255\n";
    if(size!=sizeof(header)-1+600*686 || memcmp(raster,header,sizeof(header)-1))return 0;
    int prefix=snprintf((char *)photo,64,"P5\n600 800\n255\n");
    memset(photo+prefix,255,600*800);
    memcpy(photo+prefix+600*56,raster+sizeof(header)-1,600*686);
    snprintf(path,sizeof(path),"photos/note-%d-%d-%lld.pgm",editor_note,editor_page,(long long)time(NULL));
    lumen_storage_delete("page.pgm");
    return lumen_storage_write_at(root,path,photo,prefix+600*800);
}
