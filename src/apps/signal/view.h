#include "api.h"
/* All gameplay/render state belongs to this app; shell components own chrome. */
static SignalGame signal_game;
static int signal_ready,signal_dirty,signal_frame_dirty=1;
static int64_t signal_reset_until;
static unsigned char signal_display[600*500];
static unsigned char signal_frame[SIGNAL_W*SIGNAL_H];
static void signal_save(void){
    if(signal_ready&&signal_dirty&&save_blob("signal-v1.bin",&signal_game,sizeof(signal_game)))signal_dirty=0;
}
static void signal_open(void){
    if(signal_ready)return;
    signal_ready=1;
    if(!load_blob("signal-v1.bin",&signal_game,sizeof(signal_game))||
       !isfinite(signal_game.x)||!isfinite(signal_game.z)||!isfinite(signal_game.yaw)||
       fabsf(signal_game.x)>9||signal_game.z<-10||signal_game.z>14||signal_game.lights>7||
       signal_game.yaw<0||signal_game.yaw>6.284f||(signal_game.won!=0&&signal_game.won!=1))signal_new(&signal_game);
}
static void signal_draw(void){
    signal_open();heading_actions("Signal",2);
    button_h(468,74,44,44,"?",0);icon_button(528,74,IC_RESTART);
    if(signal_frame_dirty){
        signal_render(&signal_game,signal_frame);
        for(int y=0;y<500;y++)for(int x=0;x<600;x++)
            signal_display[y*600+x]=image_bilinear(signal_frame,SIGNAL_W,SIGNAL_W,SIGNAL_H,x*128-64,y*128-64);
        signal_frame_dirty=0;
    }
    for(int y=0;y<500;y++)memcpy(ui_pixels+(132+y)*WIDTH,signal_display+y*600,600);
    /* A small paper map is both orientation and puzzle guidance. */
    rect(498,142,90,123,255);key_outline(498,142,90,123);
    rect(506,150,74,105,232);
    rect(540,154,6,98,255);
    const int house[][4]={{-6,-2,3,3},{5,-1,3,4},{-4,6,3,2},{5,7,3,3}};
    for(int i=0;i<4;i++){
        int xx=543+house[i][0]*3,yy=250-(house[i][1]+10)*4;
        rect(xx-house[i][2]*3/2,yy-house[i][3]*2,house[i][2]*3,house[i][3]*4,150);
    }
    ring(543,162,4,2);disc(543,162,2,0);
    for(int i=0;i<3;i++){
        static const int bx[]={-7,7,-3},bz[]={3,4,10};
        int x=543+bx[i]*3,y=250-(bz[i]+10)*4;
        disc(x,y,4,signal_game.lights&(1u<<i)?0:255);ring(x,y,4,2);
    }
    int px=543+(int)(signal_game.x*3),py=250-(int)((signal_game.z+10)*4);
    disc(px,py,3,0);stroke(px,py,px+(int)(sinf(signal_game.yaw)*8),py-(int)(cosf(signal_game.yaw)*8),2);
    char progress[64];int n=!!(signal_game.lights&1)+!!(signal_game.lights&2)+!!(signal_game.lights&4);
    snprintf(progress,sizeof(progress),"%d / 3",n);rect(18,146,94,38,255);type_centered(18,146,94,38,progress,UI_HEADING);
    const int kinds[]={IC_PREV,IC_UP,IC_DOWN,IC_NEXT};
    for(int i=0;i<4;i++){int x=28+i*140;region(x,650,124,52);key_outline(x,650,124,52);icon(x+40,654,kinds[i]);}
    int near=signal_near(&signal_game);
    if(signal_game.won)action_button(28,718,544,54,tr("Der Leuchtturm lebt. Geschafft!"));
    else if(near>=0)action_button(28,718,544,54,tr(near==3?"Leuchtturm entzünden":"Signal entzünden"));
    else type_centered(28,718,544,54,tr(n==3?"Zurück zum Leuchtturm":"Finde die drei Signalfeuer"),UI_SECONDARY);
}
static int signal_input(const Input *in){
    signal_open();if(in->kind!=INPUT_TAP&&in->kind!=INPUT_SWIPE)return 1;
    int changed=0;
    if(in->kind==INPUT_SWIPE){
        if(abs(in->dx)>abs(in->dy))changed=signal_move(&signal_game,0,in->dx<0?1:-1);
        else changed=signal_move(&signal_game,in->dy<0?1:-1,0);
    }else if(in->y>=74&&in->y<118){
        if(in->x>=528){
            if(signal_game.won||extra_millis()<signal_reset_until){signal_new(&signal_game);changed=1;signal_reset_until=0;}
            else {signal_reset_until=extra_millis()+3000;ui_message("Zum Neustart innerhalb von drei Sekunden nochmals tippen.");}
        }else if(in->x>=468){
            reader_markdown17("Signal",tr("Drei erloschene Signalfeuer, ein Leuchtturm und eine Insel aus Stein. Finde die drei Laternen auf der Karte. Gehe bis auf zwei Schritte heran und tippe auf Signal entzünden. Danach kehre zum hohen Leuchtturm am Nordende zurück und entzünde ihn. Links und rechts drehen dich um 15 Grad; die Pfeile in der Mitte bewegen dich vor und zurück. Auch Wischen funktioniert. Gebäude und Meer versperren den Weg. Die Karte zeigt deinen Standort und deine Blickrichtung; gefüllte Punkte sind entzündet. Kein Zeitdruck, keine Leben. Dein Spielstand wird beim Verlassen gespeichert."),129);
        }
    }else if(in->y>=650&&in->y<702){
        int i=(in->x-28)/140;if(in->x>=28&&i>=0&&i<4)changed=signal_move(&signal_game,i==1?1:i==2?-1:0,i==0?-1:i==3?1:0);
        if(!changed)ui_message("Hier ist der Weg versperrt.");
    }else if(in->y>=718&&in->y<772&&!signal_game.won)changed=signal_activate(&signal_game);
    if(changed){signal_dirty=signal_frame_dirty=1;}
    return 1;
}
