/* Output-equivalence checks for hot rendering paths and shared row contracts. */
static void reference_text(int x, int y, const char *s, const Font *f, int maxw) {
    y += ui_y_offset;
    int start = x;
    while (*s) {
        unsigned c = glyph(&s);
        if (c == '\n')
            break;
        int a = glyph_advance(f, c);
        if (x + a > start + maxw)
            break;
        const unsigned char *p = glyph_pixels(f, c);
        for (int j = 0; j < f->h && y + j < HEIGHT; j++)
            if (y + j >= 0)
                for (int i = 0; i < f->w && x + i < WIDTH; i++)
                    if (x + i >= 0 && p[j * f->w + i] < 255) {
                        int v = p[j * f->w + i], old = ui_pixels[(y + j) * WIDTH + x + i];
                        int color = ink_color == 0 && f == &caption ? UI_SECONDARY_INK
                                    : ink_color < 0                 ? 0
                                                                    : ink_color;
                        ui_pixels[(y + j) * WIDTH + x + i] = old + (color - old) * (255 - v) / 255;
                    }
        x += a;
    }
}
static void quality_tests(void) {
    static unsigned char expected[WIDTH * HEIGHT], cached[WIDTH * HEIGHT];
    const Font *faces[] = {&small, &body, &title, &editorial, &caption};
    const char *texts[] = {"Ag Wi-Fi 07:04", "Öl, cœur, français · 20%", "sin(π/3) + √25", "    ",
                           "A\nB"};
    for (int f = 0; f < 5; f++)
        for (int t = 0; t < 5; t++)
            for (int edge = 0; edge < 4; edge++) {
                int x = edge == 0   ? -9
                        : edge == 1 ? 580
                                    : 23,
                    y = edge == 2   ? -17
                        : edge == 3 ? 788
                                    : 120;
                ink_color = t == 1 ? 112 : 0;
                ui_y_offset = edge == 2 ? 3 : 0;
                memset(ui_pixels, 233, sizeof(ui_pixels));
                reference_text(x, y, texts[t], faces[f], 520);
                memcpy(expected, ui_pixels, sizeof(expected));
                memset(ui_pixels, 233, sizeof(ui_pixels));
                drawtext_raw(x, y, texts[t], faces[f], 520);
                assert(!memcmp(expected, ui_pixels, sizeof(expected)));
            }
    ink_color = ui_y_offset = 0;
    for (int h = 68; h <= 113; h++)
        for (int lines = 1; lines <= 2; lines++) {
            ListLayout l = list_layout(28, 150, 544, h, 1, 76, lines, 1, &small);
            assert(l.x == 84 && l.x + l.width <= 496 && l.icon_y >= 154);
            assert(l.detail_y >= l.title_y + l.title_h && l.detail_y + l.detail_h < 150 + h);
            assert(l.action_y >= 150 && l.action_y + 44 <= 150 + h);
        }
    /* The cache retains exact dither phase and notices atomic cover replacement. */
    Book book = {0};
    book.id = 0x123456;
    snprintf(book.title, sizeof(book.title), "Cache test");
    char dir[2048];
    snprintf(dir, sizeof(dir), "%s/covers", data_root);
    mkdir(dir, 0700);
    char path[2048];
    library_cover_path(&book, path, sizeof(path));
    unsigned char pgm[120 * 164 + 32];
    const char *header = "P5\n120 164\n255\n";
    size_t head = strlen(header);
    memcpy(pgm, header, head);
    for (int i = 0; i < 120 * 164; i++)
        pgm[head + i] = i % 239;
    assert(atomic_write(path, pgm, head + 120 * 164));
    memset(ui_pixels, 255, sizeof(ui_pixels));
    cover_size(&book, 28, 150, 140, 191);
    memcpy(cached, ui_pixels, sizeof(cached));
    memset(ui_pixels, 255, sizeof(ui_pixels));
    cover_size(&book, 28, 150, 140, 191);
    assert(!memcmp(cached, ui_pixels, sizeof(cached)));
    for (int i = 0; i < 120 * 164; i++)
        pgm[head + i] = 255 - pgm[head + i];
    assert(atomic_write(path, pgm, head + 120 * 164));
    memset(ui_pixels, 255, sizeof(ui_pixels));
    cover_size(&book, 28, 150, 140, 191);
    assert(memcmp(cached, ui_pixels, sizeof(cached)));
    unlink(path);
    cover_tiles_clear();
    route_to(63);
    assert(game_help_x() == 408);
    route_to(64);
    assert(game_help_x() == 348);
    route_to(119);
    assert(game_help_x() == 408);
    /* First render populates, repeated render reuses, activation invalidates. */
    extra_write_text("weather.json",
                     "{\"current\":{\"temperature_2m\":18,\"weather_code\":0,\"apparent_"
                     "temperature\":17,\"wind_speed_10m\":4,\"is_day\":1},\"daily\":{}}");
    route_to(32);
    extra_prepare(32);
    ui_draw();
    assert(weather_snapshot_valid && weather_snapshot);
    char *snapshot = weather_snapshot;
    ui_draw();
    assert(snapshot == weather_snapshot);
    extra_prepare(32);
    assert(!weather_snapshot_valid);
    route_to(27);keyboard=0;alarm_ring=power_menu=0;
    ui_message("Notification test");ui_draw();
    Input down={.kind=INPUT_DOWN,.x=550,.y=95},up=down;up.kind=INPUT_TAP;
    ui_input(&down);ui_input(&up);assert(screen==27 && !*notification_text());
    ui_message("Capture test");ui_draw();ui_input(&down);
    notification_dismiss();up.x=300;up.y=600;ui_input(&up);assert(screen==27);
    assert(image_contrast(128)==128 && image_contrast(80)<80 && image_contrast(180)>180);
    /* Cached compact icons must never damage the framebuffer scratch area. */
    memset(ui_pixels,237,sizeof(ui_pixels));
    icon(70,200,IC_BACK);memcpy(expected,ui_pixels,sizeof(expected));
    for(int yy=0;yy<HEIGHT;yy++)for(int xx=0;xx<WIDTH;xx++)
        if(xx<74 || xx>=110 || yy<204 || yy>=240)assert(ui_pixels[yy*WIDTH+xx]==237);
    memset(ui_pixels,237,sizeof(ui_pixels));icon(70,200,IC_BACK);
    assert(!memcmp(expected,ui_pixels,sizeof(expected)));
    assert(dropdown_option_icon(932,0,"Verschieben")==IC_MOVE);
    assert(dropdown_option_icon(932,1,"Löschen")==IC_TRASH);
    /* Native import boundary: atomic output, cached reuse and invalidation. */
    char importer[2048], source[2048], imported[2048];
    snprintf(importer,sizeof(importer),"%s/fake-converter.sh",data_root);
    snprintf(source,sizeof(source),"%s/source.kfx",data_root);
    const char *script="#!/bin/sh\nprintf 'synthetic epub fixture' > \"$2\"\n";
    assert(atomic_write(importer,script,strlen(script)));chmod(importer,0700);
    assert(atomic_write(source,"original",8));
    Book fake={.id=0x123456,.size=8,.modified=7,.protected_format=1};
    snprintf(fake.path,sizeof(fake.path),"%s",source);strcpy(fake.format,"kfx");
    setenv("LUMEN_KFX_CONVERTER",importer,1);
    assert(library_supported(&fake));assert(amazon_prepare(&fake,imported,sizeof(imported)));
    unlink(importer);unsetenv("LUMEN_KFX_CONVERTER");
    assert(amazon_prepare(&fake,imported,sizeof(imported)));
    fake.modified++;assert(!library_import_path(&fake,imported,sizeof(imported)));
    unlink(source);
    puts("PASS quality: 100 pixel-identical clipped text cases, shared row geometry, exact cover "
         "cache/invalidation, non-overlapping game actions and weather snapshot lifecycle.");
}
