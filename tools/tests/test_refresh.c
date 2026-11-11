#include "../../lumen/system/refresh.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static unsigned char before[600 * 800], after[600 * 800];
int main(void) {
    memset(before, 255, sizeof(before));
    memset(after, 255, sizeof(after));
    assert(refresh_coverage(before, after, 600, 800) == 0);
    after[0] = 0;
    after[599 + 799 * 600] = 0;
    assert(refresh_coverage(before, after, 600, 800) < 1);
    memset(after, 0, sizeof(after));
    assert(refresh_coverage(before, after, 600, 800) == 100);
    memset(after, 255, sizeof(after));
    for (int y = 0; y < 600; y++)
        memset(after + y * 600, 0, 600);
    assert(refresh_coverage(before, after, 600, 800) >= 70);
    /* Shift text strokes throughout the screen while retaining block luminance. */
    memset(after, 255, sizeof(after));
    for (int y = 0; y < 800; y += 24)
        for (int x = 0; x < 600; x += 12) {
            before[y * 600 + x] = 0;
            after[(y + 2) * 600 + x + 2] = 0;
        }
    assert(refresh_coverage(before, after, 600, 800) < 10);
    memset(before,255,sizeof(before));memset(after,255,sizeof(after));
    for(int y=0;y<780;y+=24)for(int x=0;x<588;x+=12)
        for(int j=0;j<10;j++)for(int i=0;i<3;i++){before[(y+j)*600+x+i]=0;after[(y+j+2)*600+x+i+3]=0;}
    assert(refresh_coverage(before,after,600,800)<20);
    /* A cleaned, almost-white loading frame can accept new ink in a single
       non-flashing update. A dark transition or explicit refresh still clears. */
    memset(before,255,sizeof(before));memset(after,0,sizeof(after));
    for(int y=350;y<375;y++)memset(before+y*600+285,0,30);
    RefreshDelta arrival=refresh_delta(before,after,600,800);
    assert(arrival.coverage==100 && arrival.previous_light && arrival.little_erasure);
    assert(!refresh_clear(arrival,1,0));
    assert(refresh_clear(arrival,0,0) && refresh_clear(arrival,1,1));
    RefreshDelta wipe=refresh_delta(after,before,600,800);
    assert(wipe.next_light && !wipe.little_erasure && refresh_clear(wipe,1,0));

    memset(before,255,sizeof(before));memset(after,255,sizeof(after));
    RefreshBounds same=refresh_bounds(before,after,600,800);
    assert(same.top==800 && same.left==600);
    after[40*600+10]=0;after[52*600+580]=0;after[799*600+300]=0;
    RefreshBounds sparse=refresh_bounds(before,after,600,800);
    assert(sparse.left==10 && sparse.top==40 && sparse.right==581 && sparse.bottom==800);
    puts("PASS: perceptual refresh coverage, distant tiny changes, large transitions and text-like "
         "page turns.");
}
