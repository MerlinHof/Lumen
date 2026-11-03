/* Public documentation previews use generated demo books only. */
#define _GNU_SOURCE
#include "../src/system/ui/ui.c"
#include <assert.h>
static void picture(const char *name) {
    notice[0]=0;ui_draw();char path[256];snprintf(path,sizeof(path),"build/gallery/%s.pgm",name);
    FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P5\n600 800\n255\n");fwrite(ui_pixels,1,sizeof(ui_pixels),f);fclose(f);
}
int main(void) {
    library_root="build/gallery/books";data_root="build/gallery/data";assert(engine_init());library_scan();ui_init();
    options[OPT_LANGUAGE]=1;options[OPT_KEYBOARD_LANGUAGE]=1;
    if(library_count){library[0].lastread=time(NULL);library[0].page=14;library[0].total=32;library_save_book(&library[0]);}
    update_view();route_to(1);picture("library");
    route_to(20);picture("apps");
    route_to(27);snprintf(calc_expr,sizeof(calc_expr),"sin(pi/3)^2+sqrt(5^2+3^2)");calc_cursor=strlen(calc_expr);calc_degrees=0;calc_run();picture("calculator");
    route_to(13);picture("settings");
    warehouse_new17(1);route_to(118);picture("sokoban");
    if(library_count){ui_open(0);page=engine_count()>1?1:0;reader_chrome=0;picture("reader");}
    route_to(29);note_selected=-1;extra_commit(30,"A little room to think\n\nWhat made today feel different?\n\nA walk without headphones.\nA chapter before breakfast.\nAn idea worth keeping.\n\nSmall things, written down.");route_to(30);picture("notes");
    open_keyboard(30,"Note",note_text);note_cursor=strlen(edit);picture("keyboard");keyboard=0;
    ui_close();engine_shutdown();return 0;
}
