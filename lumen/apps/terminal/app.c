#include "lumen_app.h"
#include "shell.h"
#include <fcntl.h>

enum { HISTORY=16, COMMAND=512, OUTPUT=8192 };
static char history[HISTORY][COMMAND], output[OUTPUT];
static int count, result, running, has_run;
static const char *word(const char *de,const char *en,const char *fr) {
    return lumen_language==1 ? en : lumen_language==2 ? fr : de;
}
static void draw(void) {
    ui_paint_heading("Terminal",2);
    ui_paint_icon_button(472,74,IC_HISTORY);
    ui_paint_icon_button(528,74,IC_LOCK);
    ui_paint_surface(28,135,544,52,12,182,0);
    ui_paint_surface(29,136,542,50,11,255,0);
    ui_paint_hitbox(28,135,544,52);
    lumen_text(40,135,520,52,count ? history[0] : "$",PAINT_BODY,0,1);
    ui_paint_surface(28,203,544,500,12,207,0);
    ui_paint_surface(29,204,542,498,11,255,0);
    if (running) {
        ui_paint_text(46,224,508,42,word("Wird ausgeführt …","Running …","Exécution…"),PAINT_BODY);
    } else {
        const char *start=output;
        for(int row=0;*start && row<14;row++) {
            const char *end=strchr(start,'\n');
            size_t n=end ? (size_t)(end-start) : strlen(start);
            char line[85];
            size_t chunk=n>72 ? 72 : n;
            while(chunk && ((unsigned char)start[chunk]&0xc0)==0x80)chunk--;
            if(!chunk && n)chunk=n>72 ? 72 : n;
            memcpy(line,start,chunk);line[chunk]=0;
            lumen_text(45,220+row*31,510,31,line,PAINT_BODY,0,1);
            start+=chunk;
            if (*start=='\n')start++;
        }
    }
    if(has_run) {
        char status[64];snprintf(status,sizeof(status),"Exit %d",result);
        ui_paint_text(31,718,220,35,status,PAINT_SECONDARY);
        ui_paint_icon_button(528,715,IC_BOOK);
    }
}
static void edit_command(const char *text) { lumen_request("input","Terminal",text,1); }
static int input(const Input *in) {
    if (in->kind==INPUT_TEXT && in->x==1) {
        char command[COMMAND];
        if (!lumen_string("text",command,sizeof(command)) || !*command) return 1;
        if (!count || strcmp(history[0],command)) {
            memmove(history+1,history,sizeof(history)-sizeof(history[0]));
            snprintf(history[0],sizeof(history[0]),"%s",command);
            if(count<HISTORY)count++;
            app_state_save("history.bin",history,sizeof(history));
        }
        running=1;lumen_begin();draw();lumen_end();
        result=terminal_exec(command,getenv("LUMEN_APP_DATA"),output,sizeof(output));
        running=0;has_run=1;
        return 1;
    }
    if (in->kind==INPUT_SELECT) {
        if(in->x==2 && in->y>=0 && in->y<count)edit_command(history[in->y]);
#ifndef __APPLE__
        if(in->x==3 && in->y==1) {
            if(terminal_access())unlink(TERMINAL_ACCESS);
            else { int fd=open(TERMINAL_ACCESS,O_CREAT|O_WRONLY|O_TRUNC|O_NOFOLLOW,0600);if(fd>=0)close(fd); }
        }
#endif
        return 1;
    }
    if (in->kind!=INPUT_TAP) return 1;
    if(in->y>=135 && in->y<187)edit_command("");
    if(in->y>=74 && in->y<118 && in->x>=472 && in->x<516 && count) {
        const char *choices[HISTORY];for(int i=0;i<count;i++)choices[i]=history[i];
        lumen_select(choices,count,-1,2);
    }
    if(in->y>=74 && in->y<118 && in->x>=528) {
        const char *choices[]={word("Abbrechen","Cancel","Annuler"),terminal_access() ?
            word("Webzugriff sperren","Disable web access","Fermer l’accès web") :
            word("Root-Zugriff im Web · 10 Min.","Allow web root access · 10 min","Accès root web · 10 min")};
        lumen_select(choices,2,-1,3);
    }
    if(has_run && in->y>=715 && in->y<759 && in->x>=528)lumen_request("reader","Terminal",output,0);
    return 1;
}
int main(void) {
    if(app_state_load("history.bin",history,sizeof(history)))
        while(count<HISTORY && history[count][0]) {history[count][COMMAND-1]=0;count++;}
    lumen_run(draw,input,NULL);
    return 0;
}
