#include "../../lumen/system/refresh.h"
#include <assert.h>
#include <stdio.h>

enum {W=600,H=800,BAR=56};
static unsigned char before[W*H],after[W*H];

static RefreshPlan plan(int full) {return refresh_plan(before,after,W,H,BAR,1,full);}
static void clear_frames(void) {
    memset(before,255,sizeof(before));
    memset(after,255,sizeof(after));
}
int main(void) {
    clear_frames();
    assert(plan(0).waveform==REFRESH_NONE && plan(1).waveform==REFRESH_GC16);
    assert(refresh_plan(before,after,W,H,BAR,0,0).waveform==REFRESH_CLEAR);
    after[100*W+17]=0;after[102*W+26]=0;
    RefreshPlan small=plan(0);
    assert(small.waveform==REFRESH_DU && small.area.left==16 && small.area.right==32);
    assert(small.area.top==100 && small.area.bottom==104);
    before[101*W+20]=after[101*W+20]=140;
    assert(plan(0).waveform==REFRESH_DU);
    after[101*W+20]=100;
    assert(plan(0).waveform==REFRESH_GL16);
    clear_frames();
    after[20*W+58]=0;
    RefreshPlan header=plan(0);
    assert(header.waveform==REFRESH_DU && header.area.left==56 && header.area.right==64);
    after[20*W+58]=160;
    header=plan(0);
    assert(header.area.top==0 && header.area.left==0 && header.area.right==W && header.area.bottom==BAR);
    clear_frames();
    after[0]=0;after[W*H-1]=0;
    assert(plan(0).waveform==REFRESH_GL16);
    clear_frames();
    for(int y=0;y<780;y+=24)for(int x=0;x<588;x+=12)
        for(int j=0;j<10;j++)for(int i=0;i<3;i++) {
            before[(y+j)*W+x+i]=0;
            after[(y+j+2)*W+x+i+3]=0;
        }
    assert(plan(0).waveform==REFRESH_GL16);
    clear_frames();
    /* Returning to Home adds an illustration and changes widely spaced labels,
       but removes very little ink from the old screen. */
    for(int y=65;y<415;y++)for(int x=0;x<W;x++)
        after[y*W+x]=(x+y)%7 ? 210 : 45;
    for(int x=0;x<W;x+=50)after[730*W+x]=0;
    before[10*W+60]=0;
    assert(plan(0).waveform!=REFRESH_CLEAR);
    clear_frames();
    memset(after,40,sizeof(after));
    assert(plan(0).waveform==REFRESH_GC16);
    memset(before,40,sizeof(before));memset(after,255,sizeof(after));
    assert(plan(0).waveform==REFRESH_CLEAR);
    for(int y=0;y<H;y++)for(int x=0;x<W;x++) {
        before[y*W+x]=x<W/2 ? 10 : 240;
        after[y*W+x]=x<W/2 ? 240 : 10;
    }
    assert(plan(0).waveform==REFRESH_CLEAR);
    puts("PASS shared refresh planner: sparse bounds, alignment, text motion and waveform selection.");
}
