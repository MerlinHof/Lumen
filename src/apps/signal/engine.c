/* Signal: an original CPU triangle renderer, designed for event-driven E-Ink.
 * Near-plane clipping, perspective depth, flat directional light, projected
 * shadows, material hatching and depth/normal outlines. No GPU or game library. */
#include "api.h"
#include <math.h>
#include <string.h>
#include <float.h>
#include <stdlib.h>

typedef struct {float x,y,z;} V;
typedef struct {float x,z,w,d,h;int roof;} Building;
static const Building buildings[]={
 {-6,-2,3,3,3.3,1},{5,-1,3,4,4.2,1},{-4,6,3,2,2.7,1},
 {5,7,2.5,3,3.2,1},{0,12,2.4,2.4,7,0}};
static const float beacons[3][2]={{-7,3},{7,4},{-3,10}};
static float depth[SIGNAL_W*SIGNAL_H];
static unsigned char normal[SIGNAL_W*SIGNAL_H],tone[SIGNAL_W*SIGNAL_H];
static const SignalGame *camera;
static V v(float x,float y,float z){return (V){x,y,z};}
static V sub(V a,V b){return v(a.x-b.x,a.y-b.y,a.z-b.z);}
static V cross(V a,V b){return v(a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x);}
static float dot(V a,V b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static V view(V p){
 float a=camera->yaw,c=cosf(a),s=sinf(a),dx=p.x-camera->x,dz=p.z-camera->z;
 float z=dx*s+dz*c,y=p.y-1.8f;
 return v(dx*c-dz*s,y*.9915f+z*.13f,z*.9915f-y*.13f);
}
static float edge(V a,V b,float x,float y){return (x-a.x)*(b.y-a.y)-(y-a.y)*(b.x-a.x);}
static void raster(V a,V b,V c,int shade,int face,int texture){
 V p[3]={a,b,c};
 for(int i=0;i<3;i++){float z=p[i].z;p[i].x=150+p[i].x*185/z;p[i].y=112-p[i].y*185/z;p[i].z=1/z;}
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
  if(texture && ((x+y*2)%13==0))ink-=20;
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
static V shadow(V p){return v(p.x+p.y*.65f,.025f,p.z+p.y*.4f);}
static void building(Building b){
 float l=b.x-b.w/2,r=b.x+b.w/2,f=b.z-b.d/2,k=b.z+b.d/2,h=b.h;
 quad(shadow(v(l,h,f)),shadow(v(r,h,f)),shadow(v(r,h,k)),shadow(v(l,h,k)),-154,1);
 box(b.x,0,b.z,b.w,h,b.d,226);
 if(b.roof){
  V a=v(l-.2f,h,f-.2f),c=v(r+.2f,h,f-.2f),d=v(r+.2f,h,k+.2f),e=v(l-.2f,h,k+.2f);
  V p=v(b.x,h+1.3f,f-.2f),q=v(b.x,h+1.3f,k+.2f);
  quad(a,p,q,e,142,1);quad(p,c,d,q,142,1);triangle(a,c,p,205,0);triangle(e,q,d,205,0);
 }
 /* Inset dark windows and pale lintels, rather than plain geometry blocks. */
 for(int i=0;i<2;i++){
  float xx=b.x+(i?1:-1)*b.w*.23f;
  box(xx,1.15f,f-.035f,.48f,.8f,.06f,55);
  box(xx,2.0f,f-.06f,.64f,.12f,.12f,235);
 }
 if(!b.roof){box(b.x,h,b.z,3.1f,.25f,3.1f,90);box(b.x,h+.25f,b.z,1.5f,1.2f,1.5f,camera->lights==7?255:110);box(b.x,h+1.45f,b.z,2.4f,.25f,2.4f,50);}
}
void signal_new(SignalGame *g){*g=(SignalGame){.x=0,.z=-9};}
static int clear(float x,float z){
 if(x<-9||x>9||z<-10||z>14)return 0;
 for(unsigned i=0;i<sizeof(buildings)/sizeof(*buildings);i++){
  Building b=buildings[i];if(fabsf(x-b.x)<b.w/2+.38f&&fabsf(z-b.z)<b.d/2+.38f)return 0;
 }return 1;
}
int signal_move(SignalGame *g,int forward,int turn){
 if(g->won)return 0;
 g->yaw+=turn*.2617994f;if(g->yaw>6.283185f)g->yaw-=6.283185f;if(g->yaw<0)g->yaw+=6.283185f;
 if(forward){float x=g->x+sinf(g->yaw)*forward*.85f,z=g->z+cosf(g->yaw)*forward*.85f;
 if(!clear(x,z))return 0;g->x=x;g->z=z;g->steps++;}return 1;
}
int signal_near(const SignalGame *g){
 for(int i=0;i<3;i++)if(!(g->lights&(1u<<i))&&hypotf(g->x-beacons[i][0],g->z-beacons[i][1])<2)return i;
 if(g->lights==7&&hypotf(g->x,g->z-12)<3)return 3;
 return -1;
}
int signal_activate(SignalGame *g){int i=signal_near(g);if(i<0)return 0;if(i==3)g->won=1;else g->lights|=1u<<i;return 1;}
void signal_render(const SignalGame *g,unsigned char *pixels){
 camera=g;memset(depth,0,sizeof(depth));memset(normal,0,sizeof(normal));
 for(int y=0;y<SIGNAL_H;y++)for(int x=0;x<SIGNAL_W;x++)tone[y*SIGNAL_W+x]=(unsigned char)(y<112?247:204);
 /* Ocean and wave ribbons; a tiled stone island with a pale central path. */
 quad(v(-80,-.5,-50),v(80,-.5,-50),v(80,-.5,100),v(-80,-.5,100),-202,0);
 for(int z=-20;z<50;z+=4)quad(v(-40,-.49f,z),v(40,-.49f,z),v(40,-.49f,z+.05f),v(-40,-.49f,z+.05f),-165,0);
 box(0,-.6f,2,20,.6f,26,205);
 for(int z=-10;z<15;z++)for(int x=-9;x<10;x++){
  int base=(x>=-1&&x<=1)?240:218+((x*17+z*31)&3)*4;
  quad(v(x, .005f,z),v(x+1,.005f,z),v(x+1,.005f,z+1),v(x,.005f,z+1),-base,0);
 }
 for(unsigned i=0;i<sizeof(buildings)/sizeof(*buildings);i++)building(buildings[i]);
 for(int i=0;i<3;i++){
  float x=beacons[i][0],z=beacons[i][1];int on=(g->lights&(1u<<i))!=0;
  box(x,0,z,1,.3f,1,145);box(x,.3f,z,.45f,1.4f,.45f,200);
  box(x,1.7f,z,.9f,.8f,.9f,on?250:80);box(x,2.5f,z,1.1f,.12f,1.1f,55);
 }
 /* Low stone bollards lead toward the tower; they frame the route. */
 for(int z=-6;z<12;z+=3){box(-1.8f,0,z,.25f,.65f,.25f,195);box(1.8f,0,z,.25f,.65f,.25f,195);}
 for(int y=0;y<SIGNAL_H;y++)for(int x=0;x<SIGNAL_W;x++){
  int at=y*SIGNAL_W+x,ink=tone[at];
  if(x+1<SIGNAL_W&&y+1<SIGNAL_H&&depth[at]>0){
   int neighbors[2]={at+1,at+SIGNAL_W};
   for(int i=0;i<2;i++){int b=neighbors[i];
    if(fabsf(depth[at]-depth[b])>fmaxf(.008f,depth[at]*.065f)||(normal[at]!=normal[b]&&abs((int)tone[at]-tone[b])>20))ink=28;
   }
  }
  pixels[at]=(unsigned char)ink;
 }
 /* Contrast-aware post-process AA: soften diagonal stair steps, preserve
    straight ink edges and flat paper. This is not temporal or supersample AA. */
 memcpy(tone,pixels,sizeof(tone));
 for(int y=1;y<SIGNAL_H-1;y++)for(int x=1;x<SIGNAL_W-1;x++){
  int at=y*SIGNAL_W+x,c=tone[at],l=tone[at-1],r=tone[at+1],u=tone[at-SIGNAL_W],d=tone[at+SIGNAL_W];
  if(abs(l-r)>45&&abs(u-d)>45)pixels[at]=(unsigned char)((4*c+l+r+u+d)/8);
 }

}
