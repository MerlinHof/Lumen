/* Signal: an original CPU triangle renderer, designed for event-driven E-Ink.
 * Near-plane clipping, perspective depth, flat directional light, projected
 * shadows, material hatching and depth/normal outlines. No GPU or game library. */
#include "api.h"
#include <math.h>
#include <string.h>
#include <float.h>
#include <stdlib.h>

typedef struct {float x,y,z;} V;
static float depth[SIGNAL_W*SIGNAL_H];
static unsigned char normal[SIGNAL_W*SIGNAL_H],tone[SIGNAL_W*SIGNAL_H];
static V v(float x,float y,float z){return (V){x,y,z};}
static V sub(V a,V b){return v(a.x-b.x,a.y-b.y,a.z-b.z);}
static V cross(V a,V b){return v(a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x);}
static float dot(V a,V b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static V view(V p){
 return v(p.x, p.y*.82f + (p.z-2)*.57f, 8.0f+(p.z-2)*.82f-p.y*.57f);
}
static float edge(V a,V b,float x,float y){return (x-a.x)*(b.y-a.y)-(y-a.y)*(b.x-a.x);}
static void raster(V a,V b,V c,int shade,int face,int texture){
 V p[3]={a,b,c};
 for(int i=0;i<3;i++){float z=p[i].z;p[i].x=300+p[i].x*810/z;p[i].y=190-p[i].y*810/z;p[i].z=1/z;}
 a=p[0];b=p[1];c=p[2];float area=edge(a,b,c.x,c.y);if(fabsf(area)<.001f)return;
 int x0=(int)floorf(fminf(a.x,fminf(b.x,c.x))),x1=(int)ceilf(fmaxf(a.x,fmaxf(b.x,c.x)));
 int y0=(int)floorf(fminf(a.y,fminf(b.y,c.y))),y1=(int)ceilf(fmaxf(a.y,fmaxf(b.y,c.y)));
 if(x0<0)x0=0;if(y0<0)y0=0;if(x1>=SIGNAL_W)x1=SIGNAL_W-1;if(y1>=SIGNAL_H)y1=SIGNAL_H-1;
 for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++){
  float u=edge(b,c,x+.5f,y+.5f)/area,w=edge(c,a,x+.5f,y+.5f)/area,t=1-u-w;
  if(u<0||w<0||t<0)continue;
  float iz=u*a.z+w*b.z+t*c.z;int at=y*SIGNAL_W+x;
  if(iz<=depth[at])continue;
  depth[at]=iz;normal[at]=(unsigned char)face;
  int ink=shade;
  if (texture && (x + y*2)%9 == 0) ink -= 9;
  tone[at]=(unsigned char)(ink<0?0:ink>255?255:ink);
 }
}
static void triangle(V a,V b,V c,int base,int texture){
 V n=cross(sub(b,a),sub(c,a));float len=sqrtf(dot(n,n));if(len<.0001f)return;
 n.x/=len;n.y/=len;n.z/=len;
 float light=fabsf(n.x*-.45f+n.y*.82f+n.z*-.35f);
 int shade=base<0?-base:(int)(base*(.52f+.48f*light));
 int face=1+(n.x>.5f?1:n.x<-.5f?2:n.y>.5f?3:n.y<-.5f?4:n.z>.5f?5:6);
 V input[4]={view(a),view(b),view(c)},out[5];int count=0;
 for(int i=0;i<3;i++){
  V p=input[i],q=input[(i+1)%3];int pin=p.z>=.15f,qin=q.z>=.15f;
  if(pin)out[count++]=p;
  if(pin!=qin){float t=(.15f-p.z)/(q.z-p.z);out[count++]=v(p.x+t*(q.x-p.x),p.y+t*(q.y-p.y),.15f);}
 }
 for(int i=1;i+1<count;i++)raster(out[0],out[i],out[i+1],shade,face,texture);
}
static void quad(V a,V b,V c,V d,int shade,int tex){triangle(a,b,c,shade,tex);triangle(a,c,d,shade,tex);}
static void box(float x,float y,float z,float w,float h,float d,int shade){
 float l=x-w/2,r=x+w/2,f=z-d/2,b=z+d/2,t=y+h;
 quad(v(l,y,f),v(r,y,f),v(r,t,f),v(l,t,f),shade,1);
 quad(v(r,y,b),v(l,y,b),v(l,t,b),v(r,t,b),shade,1);
 quad(v(l,y,b),v(l,y,f),v(l,t,f),v(l,t,b),shade,1);
 quad(v(r,y,f),v(r,y,b),v(r,t,b),v(r,t,f),shade,1);
 quad(v(l,t,f),v(r,t,f),v(r,t,b),v(l,t,b),shade,0);
}
void signal_project(int index, int *x, int *y) {
 V p=view(v(index%4-1.5f,.24f,3-index/4));
 *x=(int)(300+p.x*810/p.z);
 *y=(int)(190-p.y*810/p.z);
}
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
void signal_render(const SignalGame *g,unsigned char *pixels) {
 memset(depth,0,sizeof(depth));
 memset(normal,0,sizeof(normal));
 memset(tone,247,sizeof(tone));
 box(0,-.32f,1.5f,4.33f,.32f,4.33f,147);
 box(0,-.025f,1.5f,4.17f,.025f,4.17f,-208);
 for(int i=0;i<16;i++){
  float x=i%4-1.5f,z=3-i/4;
  if(g->tiles[i]){
   box(x+.06f,.008f,z+.06f,.94f,.012f,.94f,-94);
   box(x,0,z,.87f,.18f,.87f,192);
   box(x,.18f,z,.81f,.06f,.81f,250);
  }
 }
 for(int y=0;y<SIGNAL_H;y++)for(int x=0;x<SIGNAL_W;x++){
  int at=y*SIGNAL_W+x,ink=tone[at];
  if(depth[at]&&x+1<SIGNAL_W&&y+1<SIGNAL_H){
   int right=at+1,down=at+SIGNAL_W;
   if(fabsf(depth[at]-depth[right])>.004f||fabsf(depth[at]-depth[down])>.004f||
      (normal[at]!=normal[right]&&abs(tone[at]-tone[right])>26)||
      (normal[at]!=normal[down]&&abs(tone[at]-tone[down])>26))ink=25;
  }
  pixels[at]=ink;
 }
 memcpy(tone,pixels,sizeof(tone));
 for(int y=1;y<SIGNAL_H-1;y++)for(int x=1;x<SIGNAL_W-1;x++){
  int a=y*SIGNAL_W+x,l=tone[a-1],r=tone[a+1],u=tone[a-SIGNAL_W],d=tone[a+SIGNAL_W];
  if (abs(l-r)>40 && abs(u-d)>40)
   pixels[a]=(6*tone[a]+l+r+u+d)/10;
 }
}
