#include "../../lumen/apps/sliding_puzzle/api.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    SignalGame g;
    signal_new(&g, 17, 40);
    assert(!signal_won(&g) && !g.steps);
    for (int i=0;i<15;i++) g.tiles[i]=i+1;
    g.tiles[15]=0; g.empty=15;
    assert(signal_won(&g));
    assert(!signal_slide(&g,0));
    assert(signal_slide(&g,14) && !signal_won(&g));
    assert(signal_slide(&g,15) && signal_won(&g));
    puts("PASS Sliding Puzzle: solvable shuffle and legal movement");
}
