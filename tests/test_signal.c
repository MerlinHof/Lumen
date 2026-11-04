#include "../src/apps/signal/api.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static unsigned char a[SIGNAL_W*SIGNAL_H],b[SIGNAL_W*SIGNAL_H];
int main(void){
 SignalGame g;signal_new(&g);assert(g.z==-9&&g.lights==0);
 signal_render(&g,a);signal_render(&g,b);assert(!memcmp(a,b,sizeof(a)));
 int dark=0,light=0;for(unsigned i=0;i<sizeof(a);i++){dark+=a[i]<70;light+=a[i]>220;}
 assert(dark>200&&light>1000);
 signal_move(&g,0,1);signal_render(&g,b);assert(memcmp(a,b,sizeof(a)));
 g.x=-6;g.z=-4.2f;g.yaw=0;assert(!signal_move(&g,1,0));
 g.x=0;g.z=-10;g.yaw=0;assert(!signal_move(&g,-1,0));
 const float x[]={-7,7,-3},z[]={3,4,10};
 for(int i=0;i<3;i++){g.x=x[i];g.z=z[i];assert(signal_near(&g)==i);assert(signal_activate(&g));assert(!signal_activate(&g));}
 g.x=0;g.z=9.5f;assert(signal_near(&g)==3);assert(signal_activate(&g)&&g.won);
 assert(!signal_move(&g,1,0));
 for(int angle=0;angle<24;angle++){g.yaw=angle*.2617994f;signal_render(&g,b);}
 puts("PASS Signal: deterministic 3D render, camera movement, collision, all signals and lighthouse victory.");
}
