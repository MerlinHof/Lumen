#include "lumen_app.h"
typedef struct{int cells[9],turn,result,score[3];}Game;
static Game game={.turn=1};
static void save(void){app_state_save("game.bin",&game,sizeof(game));}
static void draw(void){
 ui_paint_heading("Tic Tac Toe",2);lumen_icon_action("restart",528,74,IC_RESTART);
 lumen_icon_action("help",468,74,IC_CONTENTS);
 const char*de[]={"X am Zug","O am Zug","Gewonnen!","Unentschieden"};
 const char*en[]={"X to play","O to play","Won!","Draw"};
 const char*fr[]={"Au tour de X","Au tour de O","Gagné !","Match nul"};
 const char**s=lumen_language==1?en:lumen_language==2?fr:de;
 ui_paint_text(28,145,544,48,s[game.result==3?3:game.result?2:game.turn-1],PAINT_HEADING);
 for(int i=0;i<9;i++){int x=63+i%3*160,y=241+i/3*160;
  ui_paint_hitbox(x,y,154,154);ui_paint_surface(x,y,154,154,12,218,0);
  if(game.cells[i]==1){ui_paint_line(x+40,y+40,x+114,y+114,6,0);ui_paint_line(x+114,y+40,x+40,y+114,6,0);}
  if(game.cells[i]==2)ui_paint_ring(x+77,y+77,42,6);
 }
 char b[80];snprintf(b,sizeof(b),"X  %d     ·     O  %d",game.score[1],game.score[2]);ui_paint_text(28,739,544,44,b,PAINT_BODY);
}
static int input(const Input*in){
 if(in->kind==INPUT_COMPONENT){
  char id[24],gesture[24];
  if(!lumen_string("id",id,sizeof(id)) || !lumen_string("gesture",gesture,sizeof(gesture)) || strcmp(gesture,"clicked"))return 0;
  if(!strcmp(id,"restart")){memset(game.cells,0,sizeof(game.cells));game.turn=1;game.result=0;}
  else if(!strcmp(id,"help"))lumen_request("reader","Tic Tac Toe",lumen_language==1?
   "# Tic Tac Toe\n\nTwo players take turns placing X and O. Tap an empty square. Make three in a row, column or diagonal to win. A full board without a winner is a draw. Restart begins another round and keeps the score.":lumen_language==2?
   "# Tic Tac Toe\n\nDeux joueurs placent X et O à tour de rôle. Touchez une case vide. Alignez trois symboles horizontalement, verticalement ou en diagonale. Sans gagnant, la partie est nulle. Recommencer conserve le score.":
   "# Tic Tac Toe\n\nZwei Spieler setzen abwechselnd X und O. Tippe auf ein freies Feld. Drei gleiche Zeichen in einer Reihe, Spalte oder Diagonale gewinnen. Ein volles Feld ohne Gewinner ist unentschieden. Neustart beginnt eine neue Runde und behält den Punktestand.",0);
  return 1;
 }
 if(in->kind!=INPUT_TAP)return 0;
 if(game.result||in->x<63||in->x>=537||in->y<241||in->y>=715)return 1;
 int col=(in->x-63)/160,row=(in->y-241)/160,i=row*3+col;
 if(col>2||row>2||game.cells[i])return 1;game.cells[i]=game.turn;
 static const int wins[8][3]={{0,1,2},{3,4,5},{6,7,8},{0,3,6},{1,4,7},{2,5,8},{0,4,8},{2,4,6}};
 for(int w=0;w<8;w++)if(game.cells[wins[w][0]]==game.turn&&game.cells[wins[w][1]]==game.turn&&game.cells[wins[w][2]]==game.turn)game.result=game.turn;
 if(game.result)game.score[game.turn]++;else{int n=0;for(int j=0;j<9;j++)n+=!game.cells[j];if(!n)game.result=3;}
 game.turn=3-game.turn;return 1;
}
int main(void){Game loaded;if(app_state_load("game.bin",&loaded,sizeof(loaded))){int valid=loaded.turn>=1&&loaded.turn<=2&&loaded.result>=0&&loaded.result<=3;for(int i=0;i<9;i++)valid=valid&&loaded.cells[i]>=0&&loaded.cells[i]<=2;if(valid)game=loaded;}lumen_run(draw,input,save);return 0;}
