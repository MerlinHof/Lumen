#ifndef LUMEN_GEOMETRY_H
#define LUMEN_GEOMETRY_H

/* Prefer the nearer open edge, preserving all of the selected area when it fits. */
static inline int lumen_popup_y(int top,int bottom,int min,int max,int height) {
    enum { GAP=9 };
    if(bottom<=0 || bottom<top || bottom<min || top>max)top=bottom=(min+max)/2;
    if(top<min)top=min;
    if(bottom>max)bottom=max;
    if(bottom+GAP+height<=max)return bottom+GAP;
    if(top-GAP-height>=min)return top-GAP-height;
    return top-min>max-bottom ? min : max-height;
}

#endif
