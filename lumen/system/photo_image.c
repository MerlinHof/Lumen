#include "photo_image.h"
#include "library.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned char *photo_read_raw(const char *path, int *w, int *h) {
    FILE *f=fopen(path,"rb");
    if (!f) return NULL;
    char magic[3];
    int max;
    unsigned char *pixels=NULL;
    if (fscanf(f,"%2s %d %d %d",magic,w,h,&max)==4 && !strcmp(magic,"P5") &&
        max==255 && ((*w==600 && *h==800) || (*w==800 && *h==600))) {
        int separator=fgetc(f);
        if (separator=='\r') separator=fgetc(f);
        if (separator=='\n' || separator==' ') {
            pixels=malloc(480000);
            if (pixels && fread(pixels,1,480000,f)!=480000) { free(pixels);pixels=NULL; }
        }
    }
    fclose(f);
    return pixels;
}
int photo_image_read(const char *path, unsigned char canvas[600*800]) {
    int w,h;
    unsigned char *pixels=photo_read_raw(path,&w,&h);
    if (!pixels) return 0;
    if (w==600) memcpy(canvas,pixels,480000);
    else {
        memset(canvas,255,480000);
        /* Preserve the complete landscape image; every renderer sees this rotation. */
        for (int y=0;y<450;y++)
            for (int x=0;x<600;x++)
                canvas[(y+175)*600+x]=pixels[(y*4/3)*800+x*4/3];
    }
    free(pixels);
    return 1;
}
int photo_image_rotate(const char *path) {
    int w,h;
    unsigned char *pixels=photo_read_raw(path,&w,&h);
    if (!pixels) return 0;
    unsigned char *file=malloc(480064);
    if (!file) {free(pixels);return 0;}
    int header=snprintf((char *)file,64,"P5\n%d %d\n255\n",h,w);
    for (int y=0;y<h;y++)
        for (int x=0;x<w;x++)
            file[header+x*h+(h-1-y)]=pixels[y*w+x];
    int ok=atomic_write(path,file,(size_t)header+480000);
    free(file);
    free(pixels);
    return ok;
}
