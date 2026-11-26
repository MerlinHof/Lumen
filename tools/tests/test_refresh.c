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
#include "../../lumen/system/display_viewport.h"

static void test_display_viewports(void) {
    DisplayViewport view;
    assert(!display_viewport(&view,1440,1080));
    assert(!display_viewport(&view,1,800));
    assert(display_viewport(&view,600,800));
    assert(display_damage(&view,(LumenRect){19,37,30,29}).x==19);
    assert(display_viewport(&view,1080,1440));
    assert(view.box.x==0 && view.box.y==0 && view.box.w==1080 && view.box.h==1440);
    assert(display_viewport(&view,1072,1448));
    assert(view.box.x==0 && view.box.y==9 && view.box.w==1072 && view.box.h==1429);
    assert(display_viewport(&view,1920,2560));
    assert(display_viewport(&view,800,800));
    assert(view.box.x==100 && view.box.y==0);
    assert(display_axis(-1000,-1000,1000,800)==0);
    assert(display_axis(1000,-1000,1000,800)==799);
    assert(display_to_logical(0,100,600,600)==0);
    assert(display_to_logical(799,100,600,600)==599);
    assert(display_viewport(&view,1072,1448));
    for(int y=0;y<LUMEN_SURFACE_HEIGHT;y++) {
        LumenRect box=display_damage(&view,(LumenRect){0,y,LUMEN_SURFACE_WIDTH,1});
        for(int pixel=box.y;pixel<box.y+box.h;pixel++)
            assert(display_to_logical(pixel,view.box.y,view.box.h,LUMEN_SURFACE_HEIGHT)==y);
    }
    assert(!display_contains(&view,10,0));
    assert(display_contains(&view,0,view.box.y));
    assert(!display_contains(&view,0,view.box.y+view.box.h));
    uint32_t palette[256];
    assert(display_gray_palette(16,(DisplayChannel[]){{11,5},{5,6},{0,5},{0,0}},palette));
    assert(palette[0]==0 && palette[255]==0xffff && palette[128]==0x8410);
    assert(display_gray_palette(32,(DisplayChannel[]){{16,8},{8,8},{0,8},{24,8}},palette));
    assert(palette[0]==0xff000000 && palette[128]==0xff808080 && palette[255]==0xffffffff);
    assert(!display_gray_palette(16,(DisplayChannel[]){{10,5},{9,6},{0,5},{0,0}},palette));
    LumenRect relative=lumen_fraction_rect(50,100,950,900);
    assert(relative.x==30 && relative.y==80 && relative.w==540 && relative.h==640);
}
int main(void) {
    test_display_viewports();
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
