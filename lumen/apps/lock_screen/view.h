static void lock_screen_draw(void) {
    int lock_book = current >= 0 && current < library_count ? current : library_last();
    if (options[OPT_LOCK] == 4 && v1_sleep_photo())
        return;
    if (options[OPT_LOCK] == 3) {
        char path[2048], magic[3];
        int w, h, max;
        snprintf(path, sizeof(path), "%s/lockscreen.pgm", data_root);
        FILE *f = fopen(path, "rb");
        int ok = 0;
        if (f) {
            if (fscanf(f, "%2s %d %d %d", magic, &w, &h, &max) == 4 && !strcmp(magic, "P5") &&
                w == WIDTH && h == HEIGHT && max == 255) {
                fgetc(f);
                ok = fread(ui_pixels, 1, sizeof(ui_pixels), f) == sizeof(ui_pixels);
            }
            fclose(f);
        }
        if (ok) {
            image_contrast_pixels(ui_pixels, sizeof(ui_pixels));
            return;
        }
        memset(ui_pixels, 255, sizeof(ui_pixels));
    }
    if (lock_book >= 0 && options[OPT_LOCK] == 0) {
        cover_size(&library[lock_book], 162, 113, 276, 377);
        int lines = centered_lines(44, 500, 512, library[lock_book].title, &editorial, 3);
        char author[320];
        centered_lines(44, 510 + lines * (editorial.h - 4), 512,
                       author_label(library[lock_book].author, author, sizeof(author)), &small,
                       2);
    } else if (options[OPT_LOCK] == 2) {
        time_t now = time(NULL);
        char t[32];
        strftime(t, sizeof(t), "%H:%M", localtime(&now));
        centered(28, 280, 544, 100, t, &title);
    } else
        centered(28, 300, 544, 80, "Ein Moment der Ruhe.", &editorial);
    if (options[OPT_LOCK_HINT])
        centered(28, 704, 544, 48, "Mit der Power-Taste weiterlesen", &small);
    return;
}
