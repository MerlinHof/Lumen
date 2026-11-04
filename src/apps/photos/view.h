/* App-owned module; assembled by apps/host/compose.h. */
static int photo_fill, photo_controls = 1, photo_rotation, photo_grid_page = -1;
static uint64_t photo_grid_key;
static unsigned char photo_grid[16][136 * 136];
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
    heading_actions("Fotos", 1);
    icon_button(528, 74, IC_PLUS);
    uint64_t key = photo_count ? press_id(photo_names[0]) ^ photo_count : 0;
    if (photo_grid_page != photo_page || photo_grid_key != key) {
        char dir[2048];
        local_path(dir, sizeof(dir), "photos/.thumbs");
        mkdir(dir, 0700);
        for (int r = 0; r < 16 && photo_page * 16 + r < photo_count; r++) {
            int i = photo_page * 16 + r;
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
    for (int r = 0; r < 16 && photo_page * 16 + r < photo_count; r++) {
        int x = 19 + (r % 4) * 142, y = 142 + (r / 4) * 142;
        region(x, y, 136, 136);
        for (int yy = 0; yy < 136; yy++)
            for (int xx = 0; xx < 136; xx++)
                ui_pixels[(y + yy) * 600 + x + xx] =
                    image_tone(photo_contrast(photo_grid[r][yy * 136 + xx]), x + xx, y + yy);
    }
    if (!photo_count)
        wrapped(30, 180,
                "Fotos vom Handy über LUMEN Connect hochladen. Auch Zeichnungen lassen sich als "
                "Foto speichern.",
                ui_font(UI_TITLE), 540, 5);
    pager(746, &photo_page, (photo_count + 15) / 16);
}
static int polish_photo_full(void) {
    return screen == 72 && photo_fill && !photo_controls && !v1_delete && !power_menu;
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
    int W = photo_rotation % 2 ? photo_bh : photo_bw, H = photo_rotation % 2 ? photo_bw : photo_bh;
    int area_y = photo_fill ? 0 : 52, area_h = photo_fill ? 800 : 680;
    double scale =
        photo_fill ? fmax(600. / W, (double)area_h / H) : fmin(600. / W, (double)area_h / H);
    int left = (600 - W * scale) / 2, top = area_y + (area_h - H * scale) / 2;
    rect(0, 0, 600, 800, 255);
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
            switch (photo_rotation % 4) {
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
            ui_pixels[y * 600 + x] = image_tone(photo_contrast(value), x, y);
        }
    if (photo_controls || !photo_fill) {
        rect(0, 0, 600, 50, 255);
        header();
        rect(0, 742, 600, 58, 255);
        page_arrow(28, 748, IC_PREV, photo_selected > 0);
        icon_button(148, 748, IC_ROTATE_PHOTO);
        icon_button(278, 748, IC_TRASH);
        icon_button(398, 748, IC_FULLSCREEN);
        page_arrow(528, 748, IC_NEXT, photo_selected + 1 < photo_count);
    }
}
static int polish_photo_input(const Input *in) {
    if (in->kind != INPUT_TAP)
        return 2;
    int x = in->x, y = in->y;
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
        } else if (x >= 135 && x < 210)
            photo_rotation = (photo_rotation + 1) % 4;
        else if (x >= 260 && x < 330)
            v1_delete = 1;
        else if (x >= 380 && x < 460) {
            photo_fill ^= 1;
            photo_controls = !photo_fill;
        }
    } else if (photo_fill)
        photo_controls = 0;
    return 1;
}
