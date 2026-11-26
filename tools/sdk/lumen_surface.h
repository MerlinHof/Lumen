#ifndef LUMEN_SURFACE_H
#define LUMEN_SURFACE_H

/* Layout units define the reference portrait canvas, independent of panel pixels. */
enum { LUMEN_SURFACE_WIDTH=600, LUMEN_SURFACE_HEIGHT=800, LUMEN_STATUS_HEIGHT=56 };

/* The package protocol uses signed fractions with 0.01% precision. */
enum { LUMEN_GEOMETRY_SCALE=10000 };
static inline int lumen_geometry_convert(int value,int from,int to) {
    long long scaled=(long long)value*to;
    return (int)((scaled+(scaled<0 ? -from/2 : from/2))/from);
}
static inline int lumen_geometry_encode(int value,int extent) {
    if(value < -8*extent || value > 8*extent)return 9*LUMEN_GEOMETRY_SCALE;
    return lumen_geometry_convert(value,extent,LUMEN_GEOMETRY_SCALE);
}
static inline int lumen_geometry_decode(int fraction,int extent) {
    if(fraction < -8*LUMEN_GEOMETRY_SCALE || fraction > 8*LUMEN_GEOMETRY_SCALE)
        return 9*extent;
    return lumen_geometry_convert(fraction,LUMEN_GEOMETRY_SCALE,extent);
}

typedef struct { int x,y,w,h; } LumenRect;

/* Express positions or lengths as thousandths of either logical axis. */
static inline int lumen_width_fraction(int fraction) {
    return (int)((long long)fraction*LUMEN_SURFACE_WIDTH/1000);
}
static inline int lumen_height_fraction(int fraction) {
    return (int)((long long)fraction*LUMEN_SURFACE_HEIGHT/1000);
}
static inline LumenRect lumen_fraction_rect(int left,int top,int right,int bottom) {
    int x=lumen_width_fraction(left),y=lumen_height_fraction(top);
    return (LumenRect){x,y,lumen_width_fraction(right)-x,lumen_height_fraction(bottom)-y};
}
#endif
