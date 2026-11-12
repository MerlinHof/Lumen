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
static const char *sleep_frame(int has_book) {
    static char last[256];
    static uint64_t pending_serial;
    char message[256];
    snprintf(message,sizeof(message),
             "{\"event\":\"sleep_state\",\"mode\":%d,\"book\":%d,\"hint\":%d,\"time\":%d,\"date\":%d,\"language\":%d,\"minute\":%lld}",
             options[OPT_LOCK],has_book,options[OPT_LOCK_HINT],options[OPT_LOCK_TIME],
             options[OPT_LOCK_DATE],options[OPT_LANGUAGE],(long long)(time(NULL)/60));
    if(strcmp(last,message) || app_overlay_fd(APP_OVERLAY_SLEEP)<0) {
        pending_serial=app_overlay_frame_serial(APP_OVERLAY_SLEEP);
        if(!app_overlay_send(APP_OVERLAY_SLEEP,"lumen.lock-screen",message))return "";
        snprintf(last,sizeof(last),"%s",message);
    }
    /* The power event is one of the few places where the panel must get a
       complete frame before hardware suspend. Bound waiting on the package. */
    int64_t until=extra_millis()+75;
    while(app_overlay_frame_serial(APP_OVERLAY_SLEEP)==pending_serial) {
        int left=(int)(until-extra_millis());
        if(left<=0)break;
        struct pollfd fd={app_overlay_fd(APP_OVERLAY_SLEEP),POLLIN,0};
        if(fd.fd<0 || poll(&fd,1,left)<=0)break;
        app_overlay_poll(APP_OVERLAY_SLEEP);
    }
    if(app_overlay_frame_serial(APP_OVERLAY_SLEEP)==pending_serial)return "";
    return app_overlay_frame(APP_OVERLAY_SLEEP);
}
static void sleep_surface_draw(void) {
    int selected_book=current>=0 && current<library_count ? current : library_last();
    Json frame;JsonToken tokens[128];
    if(!json_parse(&frame,sleep_frame(selected_book>=0),tokens,128)) {
        centered(28,300,544,80,"Ein Moment der Ruhe.",&editorial);
        return;
    }
    char background[24],headline[128],hint[160],clock[24],date[32];
    json_string(&frame,json_get(&frame,0,"background"),background,sizeof(background));
    json_string(&frame,json_get(&frame,0,"headline"),headline,sizeof(headline));
    json_string(&frame,json_get(&frame,0,"hint"),hint,sizeof(hint));
    json_string(&frame,json_get(&frame,0,"clock"),clock,sizeof(clock));
    json_string(&frame,json_get(&frame,0,"date"),date,sizeof(date));
    int image=0;
    if(!strcmp(background,"album"))image=v1_sleep_photo();
    else if(!strcmp(background,"photo"))image=sleep_photo_selected();
    else if(selected_book>=0 && !strcmp(background,"cover_fill")) {
        sleep_cover_fill(&library[selected_book]);image=1;
    } else if(selected_book>=0 && !strcmp(background,"cover")) {
        Book *book=&library[selected_book];char author[320];
        cover_size(book,162,113,276,377);
        int lines=centered_lines(44,500,512,book->title,&editorial,3);
        centered_lines(44,510+lines*(editorial.h-4),512,
                       author_label(book->author,author,sizeof(author)),&small,2);
        headline[0]=0;
    }
    if(!image && *headline)centered(28,300,544,80,headline,&editorial);
    if(!image && *hint)centered(28,704,544,48,hint,&small);
    double cy=json_number(&frame,json_get(&frame,0,"clock_y"));
    double dy=json_number(&frame,json_get(&frame,0,"date_y"));
    if(*clock && cy>=0 && cy<=670) {
        for(int y=-2;y<=2;y++)for(int x=-2;x<=2;x++)
            if(x*x+y*y<=5 && (x || y))type_display(20+x,(int)cy+y,560,130,clock,1,108,255);
        type_display(20,(int)cy,560,130,clock,1,108,0);
    }
    if(*date && dy>=1 && dy<=761) {
        type_display(20,(int)dy-1,560,38,date,0,25,255);
        type_display(20,(int)dy+1,560,38,date,0,25,255);
        type_display(20,(int)dy,560,38,date,0,25,0);
    }
}
