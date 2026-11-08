#include "api.h"
#include <stdlib.h>
static int adjacent(int a,int b) {
 return abs(a%4-b%4)+abs(a/4-b/4)==1;
}
int signal_slide(SignalGame *g,int index) {
 if(index<0||index>=16||!adjacent(g->empty,index))return 0;
 g->tiles[g->empty]=g->tiles[index];
 g->tiles[index]=0;
 g->empty=index;
 g->steps++;
 return 1;
}
int signal_won(const SignalGame *g) {
 for(int i=0;i<15;i++)if(g->tiles[i]!=i+1)return 0;
 return 1;
}
void signal_new(SignalGame *g,unsigned seed,int moves) {
 for(int i=0;i<15;i++)g->tiles[i]=i+1;
 g->tiles[15]=0;g->empty=15;
 int previous=-1;
 for(int i=0;i<moves;i++){
  int options[4],count=0;
  for(int j=0;j<16;j++)if(j!=previous&&adjacent(g->empty,j))options[count++]=j;
  previous=g->empty;
  seed=seed*1664525u+1013904223u;
  signal_slide(g,options[(seed>>16)%count]);
 }
 if(signal_won(g))signal_slide(g,14);
 g->steps=0;
}
