/* App-owned module; assembled by apps/host/compose.h. */
static int photo_fill, photo_controls = 1, photo_rotation, photo_grid_page = -1;
static unsigned photo_grid_key;
static unsigned char photo_grid[PHOTO_PER_PAGE][136 * 136];
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
static void polish_photos_grid(void) {
    heading_actions("Fotos", 3);
    icon_button(408, 74, IC_FOLDER_PLUS);
    icon_button(468, 74, IC_CONNECT);
    icon_button(528, 74, photo_settings ? IC_CLOSE : IC_SETTINGS);
    if (photo_settings) {
        const char *fits[] = {"Einpassen", "Ausfüllen"};
        const char *formats[] = {"Hochformat", "Querformat"};
        char seconds[32];
        snprintf(seconds, sizeof(seconds), "%d s", photo_catalog.seconds);
        select_row17(203,"Dauer pro Foto",seconds);
        select_row17(271,"Bildformat",fits[photo_catalog.fill]);
        select_row17(339,"Bildausrichtung",formats[photo_catalog.landscape]);
        wrapped(30,435,"Tippe während der Diashow, um zum Ordner zurückzukehren.",ui_font(UI_SECONDARY),540,3);
        return;
    }
    dropdown_button(28, 133, 328, 44,
                    photo_folder ? photo_catalog.folders[photo_folder - 1] : "Alle Fotos");
    if (photo_count) action_button(376, 133, 196, 44, "Diashow");
    unsigned key = photo_scan_generation;
    if (photo_grid_page != photo_page || photo_grid_key != key) {
        char dir[2048];
        local_path(dir, sizeof(dir), "photos/.thumbs");
        mkdir(dir, 0700);
        for (int r = 0; r < PHOTO_PER_PAGE && photo_page * PHOTO_PER_PAGE + r < photo_count; r++) {
            int i = photo_page * PHOTO_PER_PAGE + r;
            char path[2048], source[2048], name[300];
            snprintf(name, sizeof(name), "photos/.thumbs/%s", photo_names[i]);
            local_path(path, sizeof(path), name);
            if (access(path, R_OK)) {
                snprintf(name, sizeof(name), "photos/%s", photo_names[i]);
                local_path(source, sizeof(source), name);
                engine_photo_thumbnail(source, path);
            }
            FILE *f = fopen(path, "rb");
            int w, h, m;
            char magic[3];
            memset(photo_grid[r], 255, sizeof(photo_grid[r]));
            if (f) {
                if (fscanf(f, "%2s %d %d %d", magic, &w, &h, &m) == 4 && !strcmp(magic, "P5") &&
                    w == 136 && h == 136 && m == 255) {
                    fgetc(f);
                    fread(photo_grid[r], 1, sizeof(photo_grid[r]), f);
                }
                fclose(f);
            }
        }
        photo_grid_page = photo_page;
        photo_grid_key = key;
    }
    for (int r = 0; r < PHOTO_PER_PAGE && photo_page * PHOTO_PER_PAGE + r < photo_count; r++) {
        int x = 19 + (r % 4) * 142, y = 187 + (r / 4) * 139;
        region(x, y, 136, 136);
        widget_shape(x, y, 136, 136, RADIUS_LARGE, 3, 0);
        rounded_clip_begin(x, y, 136, 136);
        for (int yy = 0; yy < 136; yy++)
            for (int xx = 0; xx < 136; xx++)
                ui_pixels[(y + yy) * 600 + x + xx] =
                    image_tone(photo_grid[r][yy * 136 + xx], x + xx, y + yy);
        rounded_clip_end(x, y, 136, 136);
    }
    if (!photo_count)
        wrapped(30, 216, photo_folder ? "Halte ein Foto unter Alle Fotos gedrückt, um es zuzuordnen." :
                "Fotos vom Handy über LUMEN Connect hochladen.", ui_font(UI_BODY), 530, 4);
    pager(746, &photo_page, (photo_count + PHOTO_PER_PAGE - 1) / PHOTO_PER_PAGE);
}
static int photo_grid_hit(int x, int y) {
    if (x < 19 || x >= 581 || y < 187 || y >= 740 ||
        (x - 19) % 142 >= 136 || (y - 187) % 139 >= 136) return -1;
    int index = photo_page * PHOTO_PER_PAGE + (y - 187) / 139 * 4 + (x - 19) / 142;
    return index < photo_count ? index : -1;
}
static void photo_grid_input(int x, int y) {
    if (y >= 74 && y < 118) {
        if (x >= 528) photo_settings ^= 1;
        else if (x >= 468) extra_open(8);
        else if (x >= 408) open_keyboard(TEXT_INPUT_PHOTO_FOLDER, "Neuer Fotoordner", "");
        return;
    }
    if (photo_settings) {
        if (x>=360 && x<572 && y>=203 && y<251) {
            static const char *times[]={"5 s","10 s","20 s","30 s","60 s","120 s"};
            open_dropdown(905,times,6,-1,360,256,210);
        } else if (x>=360 && x<572 && y>=271 && y<319) {
            static const char *fits[]={"Einpassen","Ausfüllen"};
            open_dropdown(941,fits,2,photo_catalog.fill,360,324,210);
        } else if (x>=360 && x<572 && y>=339 && y<387) {
            static const char *formats[]={"Hochformat","Querformat"};
            open_dropdown(942,formats,2,photo_catalog.landscape,360,392,210);
        }
        return;
    }
    if (y >= 133 && y < 177) {
        if (x >= 28 && x < 356) photo_open_folder_menu(0, 0);
        else if (x >= 376 && x < 572 && photo_count) {
            photo_slideshow = 1;
            photo_selected = 0;
            photo_next_frame = extra_millis() + photo_catalog.seconds * 1000;
            extra_open(72);
        }
        return;
    }
    int index = photo_grid_hit(x, y);
    if (index >= 0) {
        photo_selected = index;
        photo_rotation = 0;
        extra_open(72);
    }
}
static int polish_photo_full(void) {
    return ui_route() == 72 && (photo_slideshow || (photo_fill && !photo_controls)) && !v1_delete && !power_menu;
}
static void photo_page_move(int delta) {
    photo_selected += delta;
    photo_rotation = 0;
}
static void polish_photo(void) {
    if (!v1_photo_load(photo_selected))
        return;
    photo_bounds();
    pagination_bind(&photo_selected, photo_count, 0, 0, photo_page_move);
    int rotation = photo_slideshow ? photo_catalog.landscape : photo_rotation;
    int fill = photo_slideshow ? photo_catalog.fill : photo_fill;
    int W = rotation % 2 ? photo_bh : photo_bw, H = rotation % 2 ? photo_bw : photo_bh;
    int full_photo = photo_slideshow || photo_fill;
    int area_y = full_photo ? 0 : 52, area_h = full_photo ? 800 : 680;
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
    if (!photo_slideshow && (photo_controls || !photo_fill)) {
        rect(0, 0, 600, 50, 255);
        header();
        rect(0, 742, 600, 58, 255);
        page_arrow(28, 748, IC_PREV, photo_selected > 0);
        icon_button(148, 748, IC_ROTATE_PHOTO);
        icon_button(216, 748, IC_FOLDER);
        icon_button(278, 748, IC_TRASH);
        icon_button(398, 748, IC_FULLSCREEN);
        page_arrow(528, 748, IC_NEXT, photo_selected + 1 < photo_count);
    }
}
static int polish_photo_input(const Input *in) {
    if (in->kind != INPUT_TAP)
        return 2;
    int x = in->x, y = in->y;
    if (photo_slideshow) {
        photo_slideshow = 0;
        extra_return(71);
        return 1;
    }
    if (photo_fill && !photo_controls) {
        photo_controls = 1;
        return 1;
    }
    if (y >= 742) {
        if (x < 100 && photo_selected) {
            photo_selected--;
            photo_rotation = 0;
        } else if (x > 500 && photo_selected + 1 < photo_count) {
            photo_selected++;
            photo_rotation = 0;
        } else if (x >= 135 && x < 210) {
            char path[2048], thumb[2048];
            snprintf(path,sizeof(path),"%s/photos/%s",data_root,photo_names[photo_selected]);
            snprintf(thumb,sizeof(thumb),"%s/photos/.thumbs/%s",data_root,photo_names[photo_selected]);
            if (photo_image_rotate(path)) {
                unlink(thumb);
                photo_scan_generation++;
                photo_loaded[0]=photo_bounds_key[0]=0;
                photo_rotation=0;
            } else ui_message("Speichern fehlgeschlagen.");
        }
        else if (x >= 216 && x < 260)
            photo_open_folder_menu(1, photo_selected);
        else if (x >= 278 && x < 322)
            v1_delete = 1;
        else if (x >= 380 && x < 460) {
            photo_fill ^= 1;
            photo_controls = !photo_fill;
        }
    } else if (photo_fill)
        photo_controls = 0;
    return 1;
}

static int photo_gallery_active(void) { return ui_route() == 71 && !photo_settings; }
static int photo_back(void) {
    if (ui_route() != 71 || !photo_settings) return 0;
    photo_settings = 0;
    return 1;
}
static void photo_app_leave(void) {
    photo_slideshow = 0;
    photo_settings = 0;
}
static void photo_duration_choice(int choice) {
    static const int durations[] = {5, 10, 20, 30, 60, 120};
    if (choice < 0 || choice >= 6) return;
    int old = photo_catalog.seconds;
    photo_catalog.seconds = durations[choice];
    if (!photo_catalog_save()) photo_catalog.seconds = old;
}
