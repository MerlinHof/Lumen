#ifndef LUMEN_DISPLAY_VIEWPORT_H
#define LUMEN_DISPLAY_VIEWPORT_H
#include <stdint.h>
#include "../../tools/sdk/lumen_surface.h"

typedef struct {
    int panel_w,panel_h;
    LumenRect box;
} DisplayViewport;

static inline int display_viewport(DisplayViewport *view,int width,int height) {
    if(width<240 || height<320 || width>8192 || height>8192 || width>height)return 0;
    int w=width,h=height;
    if((int64_t)width*LUMEN_SURFACE_HEIGHT>(int64_t)height*LUMEN_SURFACE_WIDTH)
        w=height*LUMEN_SURFACE_WIDTH/LUMEN_SURFACE_HEIGHT;
    else h=width*LUMEN_SURFACE_HEIGHT/LUMEN_SURFACE_WIDTH;
    *view=(DisplayViewport){width,height,{(width-w)/2,(height-h)/2,w,h}};
    return 1;
}
static inline int display_edge(int logical,int size,int extent,int offset) {
    return offset+(logical*size+extent-1)/extent;
}
static inline LumenRect display_damage(const DisplayViewport *view,LumenRect logical) {
    const LumenRect box=view->box;
    int x=display_edge(logical.x,box.w,LUMEN_SURFACE_WIDTH,box.x);
    int y=display_edge(logical.y,box.h,LUMEN_SURFACE_HEIGHT,box.y);
    int right=display_edge(logical.x+logical.w,box.w,LUMEN_SURFACE_WIDTH,box.x);
    int bottom=display_edge(logical.y+logical.h,box.h,LUMEN_SURFACE_HEIGHT,box.y);
    return (LumenRect){x,y,right-x,bottom-y};
}
static inline int display_contains(const DisplayViewport *view,int x,int y) {
    LumenRect box=view->box;
    return x>=box.x && x<box.x+box.w && y>=box.y && y<box.y+box.h;
}

typedef struct { unsigned shift,bits; } DisplayChannel;
static inline int display_gray_palette(int bits,const DisplayChannel channels[4],uint32_t palette[256]) {
    if(bits!=16 && bits!=24 && bits!=32)return 0;
    uint32_t occupied=0;
    for(unsigned gray=0;gray<256;gray++)palette[gray]=0;
    for(int i=0;i<4;i++) {
        DisplayChannel channel=channels[i];
        if(!channel.bits && i==3)continue;
        if(!channel.bits || channel.bits>10 || channel.shift>=(unsigned)bits ||
           channel.shift+channel.bits>(unsigned)bits)return 0;
        uint32_t white=(1u<<channel.bits)-1;
        uint32_t mask=white<<channel.shift;
        if(mask&occupied)return 0;
        occupied|=mask;
        for(unsigned gray=0;gray<256;gray++)
            palette[gray]|=i==3 ? mask : ((gray*white+127)/255)<<channel.shift;
    }
    return 1;
}
static inline int display_to_logical(int physical,int offset,int size,int extent) {
    int value=physical-offset;
    if(value<=0)return 0;
    if(value>=size)return extent-1;
    return value*extent/size;
}
static inline int display_axis(int raw,int min,int max,int panel_extent) {
    if(raw<=min)return 0;
    if(raw>=max)return panel_extent-1;
    return (int)(((int64_t)raw-min)*(panel_extent-1)/((int64_t)max-min));
}
#endif
