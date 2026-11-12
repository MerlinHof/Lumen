#ifndef LUMEN_REFRESH_H
#define LUMEN_REFRESH_H
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {int left,top,right,bottom;} RefreshBounds;
typedef enum {REFRESH_NONE=-1,REFRESH_DU,REFRESH_GL16,REFRESH_GC16,REFRESH_CLEAR} RefreshWaveform;
typedef struct {RefreshBounds area;RefreshWaveform waveform;} RefreshPlan;

static inline RefreshBounds refresh_bounds(const unsigned char *a,const unsigned char *b,int w,int h) {
    RefreshBounds box={w,h,0,0};
    for(int y=0;y<h;y++,a+=w,b+=w) {
        if(!memcmp(a,b,(size_t)w))continue;
        if(box.top==h)box.top=y;
        box.bottom=y+1;
        for(int x=0;x<box.left;x++)if(a[x]!=b[x]){box.left=x;break;}
        for(int x=w-1;x>=box.right;x--)if(a[x]!=b[x]){box.right=x+1;break;}
    }
    return box;
}

/* Global screen fractions keep margins and distant, isolated marks from
 * inflating the clearing score. Erasure measures how much old ink must be
 * removed, independently of newly darkened pixels. */
static inline RefreshWaveform refresh_large(const unsigned char *a,const unsigned char *b,int w,
                                            int h,RefreshBounds area) {
    int64_t strong=0,energy=0,erasure=0;
    for(int y=area.top;y<area.bottom;y+=16)for(int x=area.left;x<area.right;x+=16) {
        int n=0,change=0,removed=0,net=0;
        for(int j=y;j<y+16 && j<area.bottom;j++)for(int i=x;i<x+16 && i<area.right;i++) {
            int delta=b[j*w+i]-a[j*w+i];
            net+=delta;change+=abs(delta);
            if(delta>0)removed+=delta;
            n++;
        }
        energy+=change;erasure+=removed;
        if((abs(net)>=n*14 && abs(net)*2>=change) || change>=n*80)strong+=n;
    }
    int64_t pixels=(int64_t)w*h,white=pixels*255;
    if(strong*100>=pixels*40 && energy*100>=white*12 && erasure*100>=white*4)
        return REFRESH_CLEAR;
    if(strong*100>=pixels*25 && energy*100>=white*8)return REFRESH_GC16;
    return REFRESH_GL16;
}
static inline int refresh_binary(const unsigned char *a,const unsigned char *b,int w,RefreshBounds area) {
    for(int y=area.top;y<area.bottom;y++)for(int x=area.left;x<area.right;x++) {
        int old=a[y*w+x],next=b[y*w+x];
        if((old!=0 && old!=255) || (next!=0 && next!=255))return 0;
    }
    return 1;
}
static inline RefreshPlan refresh_plan(const unsigned char *a,const unsigned char *b,int w,int h,
                                       int status_height,int initialized,int full) {
    RefreshBounds whole={0,0,w,h};
    if(!initialized)return (RefreshPlan){whole,REFRESH_CLEAR};
    RefreshBounds dirty=refresh_bounds(a,b,w,h);
    if(dirty.top==h)return (RefreshPlan){whole,full ? REFRESH_GC16 : REFRESH_NONE};
    int64_t dirty_area=(int64_t)(dirty.right-dirty.left)*(dirty.bottom-dirty.top);
    int64_t pixels=(int64_t)w*h;
    RefreshWaveform mode=dirty_area*100>=pixels*40 ? refresh_large(a,b,w,h,dirty) : REFRESH_GL16;
    if(mode==REFRESH_CLEAR)return (RefreshPlan){whole,mode};
    if(full)return (RefreshPlan){whole,REFRESH_GC16};
    if(dirty.top<status_height) {
        dirty.left=0;dirty.right=w;dirty.top=0;
        if(dirty.bottom<status_height)dirty.bottom=status_height;
    }
    dirty.left&=~7;dirty.right=(dirty.right+7)&~7;
    dirty.top&=~1;dirty.bottom=(dirty.bottom+1)&~1;
    if(dirty.right>w)dirty.right=w;
    if(dirty.bottom>h)dirty.bottom=h;
    int64_t area=(int64_t)(dirty.right-dirty.left)*(dirty.bottom-dirty.top);
    if(mode==REFRESH_GL16 && dirty.top>=status_height && area*100<=pixels*18 &&
       refresh_binary(a,b,w,dirty))mode=REFRESH_DU;
    return (RefreshPlan){dirty,mode};
}
#endif
