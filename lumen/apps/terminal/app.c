#include "lumen_app.h"
#include "session.h"
#include <ctype.h>

enum { HISTORY=16, COMMAND=512, OUTPUT=8192, TOP=66, LINE=27, ADVANCE=12, COLUMNS=46 };
static char history[HISTORY][COMMAND];
static char transcript[32768];
static size_t transcript_length;
static int count,running,scroll,prompt_y,bottom=800;
static char prompt[64]="kindle > ";
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
        if(*p=='\t')col+=3-col%4;
        p++;
        while(((unsigned char)*p&0xc0)==0x80)p++;
    }
    return p;
}
static void draw(void) {
    int total=0;
    for(const char *p=transcript;*p;total++) {
        p=next_row(p);
        if(*p=='\n')p++;
    }
    int rows=(bottom-TOP)/LINE-(bottom==800);
    if(scroll>total-rows)scroll=total>rows ? total-rows : 0;
    int last=total-scroll,first=last>rows ? last-rows : 0;
    const char *start=transcript;
    for(int i=0;i<last;i++) {
        const char *end=next_row(start);
        if(i>=first) {
            char line[COLUMNS*4+1];
            size_t n=(size_t)(end-start);
            if(n>=sizeof(line))n=sizeof(line)-1;
            memcpy(line,start,n);line[n]=0;
            lumen_mono_columns(22,TOP+(i-first)*LINE,556,LINE,line,0,ADVANCE);
        }
        start=*end=='\n' ? end+1 : end;
    }
    if(bottom<800 && !total) {
        int visible=count<rows ? count : rows;
        for(int i=0;i<visible;i++)
            lumen_mono_columns(22,TOP+i*LINE,556,LINE,history[visible-1-i],96,ADVANCE);
    }
    prompt_y=TOP+(last-first)*LINE;
    if(bottom==800) {
        ui_paint_hitbox(16,prompt_y,568,LINE+8);
        lumen_mono_columns(22,prompt_y,556,LINE,running ? "…" : prompt,0,ADVANCE);
        if(!running)ui_paint_surface(22+(int)strlen(prompt)*ADVANCE,prompt_y+4,7,12,0,0,0);
    }
}
static void edit_command(const char *text) {lumen_request("input",prompt,text,1);}
static int input(const Input *in) {
    if(in->kind==INPUT_VIEWPORT){bottom=in->y==395 ? 395 : 800;return 1;}
    if(in->kind==INPUT_TEXT && in->x==1) {
        char command[COMMAND], output[OUTPUT];
        if(!lumen_string("text",command,sizeof(command)) || !*command)return 1;
        if(!count || strcmp(history[0],command)) {
            memmove(history+1,history,sizeof(history)-sizeof(history[0]));
            snprintf(history[0],sizeof(history[0]),"%s",command);
            if(count<HISTORY)count++;
            app_state_save("history.bin",history,sizeof(history));
        }
        const char *trimmed=command;
        while(isspace((unsigned char)*trimmed))trimmed++;
        if(!strncmp(trimmed,"clear",5) && trimmed[5]!='\0') {
            const char *tail=trimmed+5;
            while(isspace((unsigned char)*tail))tail++;
            if(!*tail)trimmed="clear";
        }
        if(!strcmp(trimmed,"clear")) {
            transcript[0]=0;transcript_length=0;scroll=0;
            return 1;
        }
        scroll=0;append(prompt);append(command);append("\n");
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
    if(in->kind==INPUT_SELECT && in->x==2 && in->y>=0 && in->y<count)
        edit_command(history[in->y]);
    if(in->kind==INPUT_SWIPE) {
        if(in->dy>0 && scroll<(int)sizeof(transcript))scroll+=12;
        if(in->dy<0)scroll=scroll>12 ? scroll-12 : 0;
    }
    if(in->y>=prompt_y && in->y<prompt_y+LINE+8) {
        if(in->kind==INPUT_TAP)edit_command("");
        if(in->kind==INPUT_HOLD && count) {
            const char *choices[HISTORY];for(int i=0;i<count;i++)choices[i]=history[i];
            lumen_select(choices,count,-1,2);
        }
    }
    return 1;
}
static void shutdown_shell(void){terminal_session_close(&shell);}
int main(void) {
    char hostname[52]="kindle";
    if(gethostname(hostname,sizeof(hostname)-1))snprintf(hostname,sizeof(hostname),"kindle");
    hostname[sizeof(hostname)-1]=0;
    snprintf(prompt,sizeof(prompt),"%s > ",hostname);
    if(app_state_load("history.bin",history,sizeof(history)))
        while(count<HISTORY && history[count][0]) {history[count][COMMAND-1]=0;count++;}
    atexit(shutdown_shell);
    lumen_run(draw,input,NULL);
    return 0;
}
