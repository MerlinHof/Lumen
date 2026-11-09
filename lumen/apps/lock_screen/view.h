static void lock_cover_full(Book *book) {
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
static void lock_screen_content(void) {
    int lock_book = current >= 0 && current < library_count ? current : library_last();
    if (options[OPT_LOCK] == 4 && v1_sleep_photo())
        return;
    if (options[OPT_LOCK] == 3) {
        char name[256],path[2048];
        name[0]=0;
        snprintf(path,sizeof(path),"%s/lock-photo.txt",data_root);
        FILE *file=fopen(path,"rb");
        if (file) {fgets(name,sizeof(name),file);fclose(file);}
        if (*name && !strchr(name,'/') && !strchr(name,'\\')) {
            snprintf(path,sizeof(path),"%s/photos/%s",data_root,name);
            if (photo_image_read(path,ui_pixels)) {
                image_contrast_pixels(ui_pixels,sizeof(ui_pixels));
                return;
            }
        }
    }
    if (lock_book >= 0 && options[OPT_LOCK] == 5) {
        lock_cover_full(&library[lock_book]);
        return;
    }
    if (lock_book >= 0 && options[OPT_LOCK] == 0) {
        cover_size(&library[lock_book], 162, 113, 276, 377);
        int lines = centered_lines(44, 500, 512, library[lock_book].title, &editorial, 3);
        char author[320];
        centered_lines(44, 510 + lines * (editorial.h - 4), 512,
                       author_label(library[lock_book].author, author, sizeof(author)), &small,
                       2);
    } else if (options[OPT_LOCK] != 2)
        centered(28, 300, 544, 80, "Ein Moment der Ruhe.", &editorial);
    if (options[OPT_LOCK_HINT])
        centered(28, 704, 544, 48, "Mit der Power-Taste weiterlesen", &small);
    return;
}

/* Overlay belongs to the lock-screen app and applies to every content mode. */
static void lock_screen_draw(void) {
    lock_screen_content();
    int clock_mode = options[OPT_LOCK] == 2;
    if (!clock_mode && !options[OPT_LOCK_TIME] && !options[OPT_LOCK_DATE]) return;
    time_t now = time(NULL);
    struct tm date;
    localtime_r(&now, &date);
    char text[32];
    if (clock_mode || options[OPT_LOCK_TIME]) {
        strftime(text, sizeof(text), "%H:%M", &date);
        int y = clock_mode ? 285 : 18;
        for (int dy=-2; dy<=2; dy++)
            for (int dx=-2; dx<=2; dx++)
                if (dx*dx+dy*dy<=5 && (dx || dy))
                    type_display(20+dx,y+dy,560,130,text,1,108,255);
        type_display(20,y,560,130,text,1,108,0);
    }
    if (options[OPT_LOCK_DATE]) {
        strftime(text, sizeof(text), "%d.%m.%Y", &date);
        int y = clock_mode ? 418 : options[OPT_LOCK_TIME] ? 136 : 22;
        type_display(20,y-1,560,38,text,0,25,255);
        type_display(20,y+1,560,38,text,0,25,255);
        type_display(20,y,560,38,text,0,25,0);
    }
}
