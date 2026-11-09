#include "lumen_app.h"
#include "session.h"
#include "shell.h"
#include <fcntl.h>

enum { HISTORY=16, COMMAND=512, OUTPUT=8192, ROWS=24, COLUMNS=45 };
static char history[HISTORY][COMMAND];
static char transcript[32768];
static size_t transcript_length;
static int count, running, scroll;
static TerminalSession shell;

static const char *word(const char *de,const char *en,const char *fr) {
    return lumen_language==1 ? en : lumen_language==2 ? fr : de;
}
static void append(const char *text) {
    size_t n=strlen(text);
    if(n>=sizeof(transcript))return;
    if(transcript_length+n>=sizeof(transcript)) {
        size_t drop=transcript_length+n-sizeof(transcript)+1;
        while(drop<transcript_length && transcript[drop-1]!='\n')drop++;
        memmove(transcript,transcript+drop,transcript_length-drop);
        transcript_length-=drop;
    }
    memcpy(transcript+transcript_length,text,n+1);
    transcript_length+=n;
}
static const char *next_row(const char *p) {
    for(int col=0;*p && *p!='\n' && col<COLUMNS;col++) {
        p++;
        while(((unsigned char)*p&0xc0)==0x80)p++;
    }
    return p;
}
static void draw(void) {
    ui_paint_heading("Terminal",2);
    ui_paint_icon_button(472,74,IC_HISTORY);
    ui_paint_icon_button(528,74,IC_LOCK);
    ui_paint_line(28,127,572,127,1,177);
    int total=0;
    for(const char *p=transcript;*p;total++) {
        p=next_row(p);
        if(*p=='\n')p++;
    }
    if(scroll>total-ROWS)scroll=total>ROWS ? total-ROWS : 0;
    int last=total-scroll,first=last>ROWS ? last-ROWS : 0;
    const char *start=transcript;
    for(int i=0;i<last;i++) {
        const char *end=next_row(start);
        if(i>=first) {
            char line[COLUMNS*4+1];
            size_t n=(size_t)(end-start);
            if(n>=sizeof(line))n=sizeof(line)-1;
            memcpy(line,start,n);line[n]=0;
            lumen_mono(30,133+(i-first)*24,540,24,line,0);
        }
        start=*end=='\n' ? end+1 : end;
    }
    ui_paint_line(28,718,572,718,1,177);
    ui_paint_hitbox(28,721,544,57);
    lumen_mono(30,724,36,48,"$",0);
    lumen_mono(65,724,502,48,running ? "…" :
        word("Befehl eingeben","Enter a command","Saisir une commande"),118);
}
static void edit_command(const char *text) {lumen_request("input","Terminal",text,1);}
static int input(const Input *in) {
    if(in->kind==INPUT_TEXT && in->x==1) {
        char command[COMMAND], output[OUTPUT];
        if(!lumen_string("text",command,sizeof(command)) || !*command)return 1;
        if(!count || strcmp(history[0],command)) {
            memmove(history+1,history,sizeof(history)-sizeof(history[0]));
            snprintf(history[0],sizeof(history[0]),"%s",command);
            if(count<HISTORY)count++;
            app_state_save("history.bin",history,sizeof(history));
        }
        scroll=0;append("$ ");append(command);append("\n");
        running=1;lumen_begin();draw();lumen_end();
        int result=terminal_session_command(&shell,access("/mnt/us",X_OK) ? "/" : "/mnt/us",
                                            command,output,sizeof(output));
        append(output);
        if(*output && output[strlen(output)-1]!='\n')append("\n");
        if(result) {
            char status[100];
            if(result==124)snprintf(status,sizeof(status),"%s",
                word("[Zeitlimit · Shell geschlossen]\n","[Timed out · shell closed]\n","[Délai dépassé · shell fermé]\n"));
            else if(result==125 && !shell.pid)snprintf(status,sizeof(status),"%s",
                word("[Shell geschlossen]\n","[Shell closed]\n","[Shell fermé]\n"));
            else snprintf(status,sizeof(status),"[exit %d]\n",result);
            append(status);
        }
        running=0;return 1;
    }
    if(in->kind==INPUT_SELECT) {
        if(in->x==2 && in->y>=0 && in->y<count)edit_command(history[in->y]);
#ifndef __APPLE__
        if(in->x==3 && in->y==1) {
            if(terminal_access())unlink(TERMINAL_ACCESS);
            else {int fd=open(TERMINAL_ACCESS,O_CREAT|O_WRONLY|O_TRUNC|O_NOFOLLOW,0600);if(fd>=0)close(fd);}
        }
#endif
        return 1;
    }
    if(in->kind==INPUT_SWIPE) {
        if(in->dy>0 && scroll<sizeof(transcript))scroll+=ROWS-3;
        if(in->dy<0)scroll=scroll>ROWS-3 ? scroll-(ROWS-3) : 0;
        return 1;
    }
    if(in->kind!=INPUT_TAP)return 1;
    if(in->y>=721 && in->y<778)edit_command("");
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
    return 1;
}
static void shutdown_shell(void){terminal_session_close(&shell);}
int main(void) {
    if(app_state_load("history.bin",history,sizeof(history)))
        while(count<HISTORY && history[count][0]) {history[count][COMMAND-1]=0;count++;}
    atexit(shutdown_shell);
    lumen_run(draw,input,NULL);
    return 0;
}
