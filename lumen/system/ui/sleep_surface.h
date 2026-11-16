/* Device image sources and typography for an independent sleep overlay. */
static void sleep_cover_fill(Book *book) {
    static unsigned char pixels[WIDTH*HEIGHT];
    static uint64_t id;
    static int64_t modified;
    static int rendered;
    if (id != book->id || modified != book->modified) {
        char path[2048];
        int imported = library_import_path(book,path,sizeof(path));
        rendered = (imported || !strcmp(book->format,"epub") || !strcmp(book->format,"pdf")) &&
                   engine_cover_fill(imported ? path : book->path,pixels,WIDTH,HEIGHT);
        id = book->id;
        modified = book->modified;
    }
    if (!rendered) {
        cover_draw(book,0,0,WIDTH,HEIGHT,1);
        return;
    }
    int x0,y0,x1,y1;
    cover_bounds(pixels,WIDTH,HEIGHT,&x0,&y0,&x1,&y1,1);
    int w=x1-x0,h=y1-y0;
    if (w*HEIGHT > h*WIDTH) {
        int fit=h*WIDTH/HEIGHT;
        x0+=(w-fit)/2;
        w=fit;
    } else {
        int fit=w*HEIGHT/WIDTH;
        y0+=(h-fit)/2;
        h=fit;
    }
    for (int y=0;y<HEIGHT;y++)
        for (int x=0;x<WIDTH;x++) {
            int tone=image_bilinear(pixels+y0*WIDTH+x0,WIDTH,w,h,
                                    (2*x+1)*w*128/WIDTH-128,(2*y+1)*h*128/HEIGHT-128);
            ui_pixels[y*WIDTH+x]=theme_gray(image_tone(tone,x,y));
        }
}

static int sleep_photo_selected(void) {
    char name[256]="",path[2048];
    snprintf(path,sizeof(path),"%s/lock-photo.txt",data_root);
    FILE *file=fopen(path,"rb");
    if(file){fgets(name,sizeof(name),file);fclose(file);}
    if(!*name || name[0]=='.' || strchr(name,'/') || strchr(name,'\\'))return 0;
    snprintf(path,sizeof(path),"%s/photos/%s",data_root,name);
    if(!photo_image_read(path,ui_pixels))return 0;
    image_contrast_pixels(ui_pixels,sizeof(ui_pixels));
    return 1;
}
static const char *sleep_frame(int book) {
    static char last[1024];
    static uint64_t pending_serial;
    char *state=NULL;
    size_t length=0;
    FILE *out=open_memstream(&state,&length);
    if(!out)return "";
    fprintf(out,"{\"event\":\"sleep_state\",\"mode\":%d,\"book\":%d,\"hint\":%d,\"time\":%d,\"date\":%d,\"language\":%d,\"minute\":%lld",
            options[OPT_LOCK],book>=0,options[OPT_LOCK_HINT],options[OPT_LOCK_TIME],
            options[OPT_LOCK_DATE],options[OPT_LANGUAGE],(long long)(time(NULL)/60));
    if(book>=0) {
        char author[320];
        package_field(out,"title",library[book].title);
        package_field(out,"author",author_label(library[book].author,author,sizeof(author)));
    }
    fputc('}',out);
    if(fclose(out) || length>=sizeof(last)){free(state);return "";}
    if(strcmp(last,state) || app_overlay_fd(app_provider_overlay_slot(PROVIDER_SLEEP))<0) {
        pending_serial=app_overlay_frame_serial(app_provider_overlay_slot(PROVIDER_SLEEP));
        if(!app_provider_overlay_send(PROVIDER_SLEEP,state)){free(state);return "";}
        snprintf(last,sizeof(last),"%s",state);
    }
    free(state);
    /* The power event is one of the few places where the panel must get a
       complete frame before hardware suspend. Bound waiting on the package. */
    int64_t until=extra_millis()+75;
    while(app_overlay_frame_serial(app_provider_overlay_slot(PROVIDER_SLEEP))==pending_serial) {
        int left=(int)(until-extra_millis());
        if(left<=0)break;
        struct pollfd fd={app_overlay_fd(app_provider_overlay_slot(PROVIDER_SLEEP)),POLLIN,0};
        if(fd.fd<0 || poll(&fd,1,left)<=0)break;
        app_overlay_poll(app_provider_overlay_slot(PROVIDER_SLEEP));
    }
    if(app_overlay_frame_serial(app_provider_overlay_slot(PROVIDER_SLEEP))==pending_serial)return "";
    return app_overlay_frame(app_provider_overlay_slot(PROVIDER_SLEEP));
}
static int system_image_book;
static void sleep_device_image(const char *source,int x,int y,int w,int h) {
    if(!strcmp(source,"document_cover") && system_image_book>=0 &&
       x>=0 && y>=0 && w>=25 && h>=30 && w<=WIDTH-x && h<=HEIGHT-y)
        cover_size(&library[system_image_book],x,y,w,h);
    if(x || y || w!=WIDTH || h!=HEIGHT)return;
    if(!strcmp(source,"wallpaper_album"))v1_sleep_photo();
    else if(!strcmp(source,"wallpaper"))sleep_photo_selected();
    else if(!strcmp(source,"document_cover_fill") && system_image_book>=0)
        sleep_cover_fill(&library[system_image_book]);
}
static void sleep_surface_draw(void) {
    system_image_book=current>=0 && current<library_count ? current : library_last();
    const char *frame=sleep_frame(system_image_book);
    if(!*frame) {
        centered(28,300,544,80,"Ein Moment der Ruhe.",&editorial);
        return;
    }
    overlay_image=sleep_device_image;
    overlay_paint(frame,(OverlayBounds){0,0,WIDTH,HEIGHT},NULL);
    overlay_image=NULL;
}
