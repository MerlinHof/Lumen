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

/* Compare block luminance, not just the bounding rectangle: two distant tiny
 * changes and ordinary text page turns must not imply a whole-screen flash.
 * Persistent empty white margins do not dilute a large visible-content change. */
static inline int refresh_coverage(const unsigned char *a, const unsigned char *b, int w, int h) {
    long changed_area = 0, content_area = 0;
    for (int y = 0; y < h; y += 32)
        for (int x = 0; x < w; x += 24) {
            int n = 0, sa = 0, sb = 0, energy = 0;
            for (int j = y; j < y + 32 && j < h; j++)
                for (int i = x; i < x + 24 && i < w; i++) {
                    int u = a[j * w + i], v = b[j * w + i];
                    sa += u;
                    sb += v;
                    energy += abs(u - v);
                    n++;
                }
            if (sa < n * 250 || sb < n * 250)
                content_area += n;
            if (abs(sa - sb) >= n * 12 || energy >= n * 80)
                changed_area += n;
        }
    return content_area ? (int)(100 * changed_area / content_area) : 0;
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
    if(dirty_area*100>=pixels*60 && refresh_coverage(a,b,w,h)>=70)
        return (RefreshPlan){whole,REFRESH_CLEAR};
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
    RefreshWaveform mode=dirty.top<status_height ? REFRESH_GC16 : REFRESH_GL16;
    if(mode==REFRESH_GL16 && area<pixels/4 && refresh_binary(a,b,w,dirty))mode=REFRESH_DU;
    return (RefreshPlan){dirty,mode};
}
#endif
