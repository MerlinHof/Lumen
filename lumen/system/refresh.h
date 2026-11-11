#ifndef LUMEN_REFRESH_H
#define LUMEN_REFRESH_H
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct { int left,top,right,bottom; } RefreshBounds;
typedef struct {
    int coverage;
    int previous_light,next_light;
    int little_erasure;
} RefreshDelta;

/* Skip unchanged rows using the platform's optimized byte comparison. Once
 * horizontal bounds are known, only check for changes outside those bounds. */
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

/* Perceptual coverage ignores margins and low-energy text movement. Ink mass
 * and erasure distinguish drawing on clean paper from replacing old content. */
static inline RefreshDelta refresh_delta(const unsigned char *a,const unsigned char *b,int w,int h) {
    int64_t changed_area=0,content_area=0,old_ink=0,new_ink=0,erased=0;
    for(int y=0;y<h;y+=32)for(int x=0;x<w;x+=24) {
        int n=0,sa=0,sb=0,energy=0,block_erased=0;
        for(int j=y;j<y+32 && j<h;j++)for(int i=x;i<x+24 && i<w;i++) {
            int old=a[j*w+i],next=b[j*w+i],delta=next-old;
            sa+=old;sb+=next;energy+=abs(delta);
            if(delta>0)block_erased+=delta;
            n++;
        }
        erased+=block_erased;
        old_ink+=(int64_t)n*255-sa;new_ink+=(int64_t)n*255-sb;
        if(sa<n*250 || sb<n*250)content_area+=n;
        if(abs(sa-sb)>=n*12 || energy>=n*80)changed_area+=n;
    }
    int64_t white=(int64_t)w*h*255;
    return (RefreshDelta){
        .coverage=content_area ? (int)(100*changed_area/content_area) : 0,
        .previous_light=old_ink*40<=white,
        .next_light=new_ink*40<=white,
        .little_erasure=erased*128<=white
    };
}
static inline int refresh_coverage(const unsigned char *a,const unsigned char *b,int w,int h) {
    return refresh_delta(a,b,w,h).coverage;
}
static inline int refresh_clear(RefreshDelta change,int clean_paper,int explicit_refresh) {
    return change.coverage>=70 &&
        (explicit_refresh || !clean_paper || !change.previous_light || !change.little_erasure);
}
#endif
