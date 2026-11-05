#include "../../lumen/apps/slide_3d/api.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned char a[SIGNAL_W*SIGNAL_H],b[SIGNAL_W*SIGNAL_H];
int main(void) {
    SignalGame g;
    signal_new(&g, 17, 40);
    assert(!signal_won(&g) && !g.steps);
    signal_render(&g,a);
    signal_render(&g,b);
    assert(!memcmp(a,b,sizeof(a)));
    for (int i=0;i<15;i++) g.tiles[i]=i+1;
    g.tiles[15]=0; g.empty=15;
    assert(signal_won(&g));
    assert(!signal_slide(&g,0));
    assert(signal_slide(&g,14) && !signal_won(&g));
    assert(signal_slide(&g,15) && signal_won(&g));
    puts("PASS Slide 3D: deterministic render and legal puzzle movement");
}
