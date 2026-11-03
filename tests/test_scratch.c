#include "../src/scratch.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
int main(void){ScratchLine r[128];const char*s="# worksheet\na = 100\nb = 50\na-b\n*5\ndistance = 100 km\ndistance to miles\nexpenses = [10,20,30]\navg expenses\nsum(expenses)\nsqrt(144)\nsin(pi/2)\n100 km / 2 h to km/h\n1 m + 1 s\n1/0\na = 200\na+b";int n=scratch_evaluate(s,r,128);assert(n==17);assert(!strcmp(r[4].result,"250"));assert(fabs(atof(r[6].result)-62.13711922)<.00001);assert(!strcmp(r[8].result,"20"));assert(!strcmp(r[9].result,"60"));assert(!strcmp(r[10].result,"12"));assert(!strcmp(r[11].result,"1"));assert(!strcmp(r[12].result,"50 km/h"));assert(r[13].state==-1&&r[14].state==-1);assert(!strcmp(r[16].result,"250"));scratch_evaluate("price=120\ntax=price*19%\nprice+tax",r,128);assert(!strcmp(r[2].result,"142.8"));puts("PASS: arithmetic, edited variables, continuation, functions, lists, units, conversions and invalid dimensions/division.");}
