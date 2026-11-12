static int photo_bx, photo_by, photo_bw = 600, photo_bh = 800;
static char photo_bounds_key[256];
static void photo_bounds(void) {
    if (!strcmp(photo_bounds_key, photo_loaded))
        return;
    snprintf(photo_bounds_key, 256, "%s", photo_loaded);
    int top = 0, bottom = 799, left = 0, right = 599;
    for (; top < 799; top++) {
        int content = 0;
        for (int x = 0; x < 600; x++)
            if (photo_pixels[top * 600 + x] < 247) {
                content = 1;
                break;
            }
        if (content)
            break;
    }
    for (; bottom > top; bottom--) {
        int content = 0;
        for (int x = 0; x < 600; x++)
            if (photo_pixels[bottom * 600 + x] < 247) {
                content = 1;
                break;
            }
        if (content)
            break;
    }
    for (; left < 599; left++) {
        int content = 0;
        for (int y = top; y <= bottom; y++)
            if (photo_pixels[y * 600 + left] < 247) {
                content = 1;
                break;
            }
        if (content)
            break;
    }
    for (; right > left; right--) {
        int content = 0;
        for (int y = top; y <= bottom; y++)
            if (photo_pixels[y * 600 + right] < 247) {
                content = 1;
                break;
            }
        if (content)
            break;
    }
    if (right - left < 10 || bottom - top < 10) {
        left = top = 0;
        right = 599;
        bottom = 799;
    }
    photo_bx = left;
    photo_by = top;
    photo_bw = right - left + 1;
    photo_bh = bottom - top + 1;
}
static void photo_surface(int index,int fill,int rotation,int full_photo) {
    if(!v1_photo_load(index))return;
    photo_bounds();
    int W = rotation % 2 ? photo_bh : photo_bw, H = rotation % 2 ? photo_bw : photo_bh;
    int area_y = full_photo ? 0 : STATUS_HEIGHT, area_h = full_photo ? 800 : 680;
    double scale =
        fill ? fmax(600. / W, (double)area_h / H) : fmin(600. / W, (double)area_h / H);
    int left = (600 - W * scale) / 2, top = area_y + (area_h - H * scale) / 2;
    rect(0, 0, 600, 800, 255);
    int drawn_w = (int)ceil(W * scale), drawn_h = (int)ceil(H * scale);
    int clip = !fill && drawn_w >= 18 && drawn_h >= 18;
    if (clip) rounded_clip_begin(left, top, drawn_w, drawn_h);
    int xmap[600], ymap[800];
    for (int x = 0; x < 600; x++)
        xmap[x] = x < left ? -1 : (int)(((x - left + .5) / scale - .5) * 256);
    for (int y = area_y; y < area_y + area_h; y++)
        ymap[y] = y < top ? -1 : (int)(((y - top + .5) / scale - .5) * 256);
    for (int y = area_y; y < area_y + area_h; y++)
        for (int x = 0; x < 600; x++) {
            int u = xmap[x], v = ymap[y];
            if (x < left || y < top || x >= left + W * scale || y >= top + H * scale)
                continue;
            if (u < 0)
                u = 0;
            if (v < 0)
                v = 0;
            int a, b;
            switch (rotation % 4) {
            case 1:
                a = v;
                b = (photo_bh - 1) * 256 - u;
                break;
            case 2:
                a = (photo_bw - 1) * 256 - u;
                b = (photo_bh - 1) * 256 - v;
                break;
            case 3:
                a = (photo_bw - 1) * 256 - v;
                b = u;
                break;
            default:
                a = u;
                b = v;
            }
            int value = image_bilinear(photo_pixels + photo_by * 600 + photo_bx, 600, photo_bw,
                                       photo_bh, a, b);
            ui_pixels[y * 600 + x] = image_tone(value, x, y);
        }
    if (clip) rounded_clip_end(left, top, drawn_w, drawn_h);
}

typedef struct {
    char name[256];
    unsigned char pixels[136*136];
} PhotoThumbnail;
static PhotoThumbnail thumbnails[16];
static unsigned next_thumbnail;
static void photo_thumbnail(int index,int x,int y) {
    if(index<0 || index>=photo_count)return;
    PhotoThumbnail *tile=NULL;
    const char *name=photo_names[index];
    for(int i=0;i<16;i++)if(!strcmp(thumbnails[i].name,name)){tile=&thumbnails[i];break;}
    if(!tile) {
        char dir[2048],path[2048],source[2048];
        local_path(dir,sizeof(dir),"photos/.thumbs");mkdir(dir,0700);
        snprintf(path,sizeof(path),"%s/%s",dir,name);
        snprintf(source,sizeof(source),"%s/photos/%s",data_root,name);
        if(access(path,R_OK) && !engine_photo_thumbnail(source,path))return;
        FILE *f=fopen(path,"rb");
        if(!f)return;
        int w,h,max;char magic[3];
        tile=&thumbnails[next_thumbnail++%16];tile->name[0]=0;
        int ok=fscanf(f,"%2s %d %d %d",magic,&w,&h,&max)==4 && !strcmp(magic,"P5") &&
               w==136 && h==136 && max==255 && fgetc(f)!=EOF &&
               fread(tile->pixels,1,sizeof(tile->pixels),f)==sizeof(tile->pixels);
        fclose(f);
        if(!ok)return;
        snprintf(tile->name,sizeof(tile->name),"%s",name);
    }
    rounded_clip_begin(x,y,136,136);
    for(int r=0;r<136;r++)for(int c=0;c<136;c++)
        ui_pixels[(y+r)*WIDTH+x+c]=image_tone(tile->pixels[r*136+c],x+c,y+r);
    rounded_clip_end(x,y,136,136);
}
static void photo_invalidate_image(const char *name) {
    char cache[2048];snprintf(cache,sizeof(cache),"%s/photos/.thumbs/%s",data_root,name);
    unlink(cache);
    for(int i=0;i<16;i++)if(!strcmp(thumbnails[i].name,name))thumbnails[i].name[0]=0;
    photo_loaded[0]=0;photo_bounds_key[0]=0;
}
