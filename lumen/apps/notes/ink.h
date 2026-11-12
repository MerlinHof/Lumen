static const char *note_next_line(const char *p, int width) {
    const char *q = p, *space = NULL;
    int w = 0;
    while (*q && *q != '\n') {
        const char *old = q;
        unsigned c = glyph(&q);
        if (w + glyph_advance(ui_font(UI_BODY), c) > width) {
            q = space && space > p ? space : old;
            break;
        }
        w += glyph_advance(ui_font(UI_BODY), c);
        if (c == ' ')
            space = q;
    }
    if (q == p && *q && *q != '\n')
        glyph(&q);
    if (*q == '\n')
        q++;
    return q;
}
/* App-owned module; assembled by apps/host/compose.h. */
static int folder15_index(int id) {
    for (int i = 0; i < note_tree.nf; i++)
        if (note_tree.folder[i].id == id)
            return i;
    return -1;
}
static int folder15_inside(int id, int root) {
    for (int i = 0; i < 513 && id; i++) {
        if (id == root)
            return 1;
        int f = folder15_index(id);
        if (f < 0)
            break;
        id = note_tree.folder[f].parent;
    }
    return 0;
}
static void note15_layout(const char *text) {
    const char *p = text;
    note_rows = 0;
    int line = 1;
    while (*p && note_rows < 8192) {
        note_offsets[note_rows] = p - text;
        note_numbers[note_rows] = line;
        const char *q = note_next_line(
            p, editor_ui.keyboard && editor_ui.keyboard_target != 30 ? 544 : (note_rows % 23 < 2 ? 420 : 528));
        if (q <= p)
            break;
        if (q[-1] == '\n')
            line++;
        p = q;
        note_rows++;
    }
    if (note_rows && p > text && p[-1] == '\n' && note_rows < 8192) {
        note_offsets[note_rows] = p - text;
        note_numbers[note_rows++] = line;
    }
    note_offsets[note_rows] = p - text;
    if (!note_rows) {
        note_numbers[0] = 1;
        note_rows = 1;
        note_offsets[1] = 0;
    }
}
static int u15_note_page_count(void) {
    note15_layout(note_text);
    return (note_rows + 22) / 23;
}
static void u15_note_editor_draw(void) {
    const char *text = editor_ui.keyboard && keyboard_multiline() ? editor_ui.edit : note_text;
    note15_layout(text);
    int editing = editor_ui.keyboard && keyboard_multiline(), rows = editing ? 15 : 23, cursor_row = 0;
    for (int i = 0; i < note_rows; i++)
        if (editor_ui.note_cursor >= note_offsets[i])
            cursor_row = i;
    if (editing && !editor_ui.note_scroll_lock) {
        if (cursor_row < editor_ui.note_view_start)
            editor_ui.note_view_start = cursor_row;
        if (cursor_row >= editor_ui.note_view_start + rows)
            editor_ui.note_view_start = cursor_row - rows + 1;
    }
    if (!editing)
        editor_ui.note_view_start = extra_page * 23;
    int left = editing && editor_ui.keyboard_target != 30 ? 28 : 48;
    for (int r = 0; r < rows; r++)
        rect(left, 78 + r*29, 572-left, 1, 231);
    for (int r = 0; r < rows && editor_ui.note_view_start + r < note_rows; r++) {
        int i = editor_ui.note_view_start + r;
        char b[1024];
        int n = note_offsets[i + 1] - note_offsets[i];
        if (n > 1023)
            n = 1023;
        memcpy(b, text + note_offsets[i], n);
        b[n] = 0;
        char num[20];
        snprintf(num, 20, "%d", note_numbers[i]);
        if (i > 0 && note_numbers[i] == note_numbers[i - 1])
            num[0] = 0;
        ink_color = UI_SECONDARY_INK;
        if (left == 48)
            small_number17(24, 54 + r * 29, num, 135);
        ink_color = 0;
        drawtext_raw(left, 54 + r * 29, b, ui_font(UI_BODY),
                     editing && editor_ui.keyboard_target != 30 ? 544 : (i % 23 < 2 ? 420 : 528));
        if (editing && cursor_row == i && editor_ui.cursor_visible) {
            snprintf(b, sizeof(b), "%.*s", editor_ui.note_cursor - note_offsets[i], text + note_offsets[i]);
            text_caret(left + textwidth_raw(b, ui_font(UI_BODY)), 54 + r * 29, ui_font(UI_BODY));
        }
    }
}
static void u15_note_scroll(int delta) {
    note15_layout(editor_ui.edit);
    int last = note_rows > 15 ? note_rows-15 : 0;
    int first = editor_ui.note_view_start + delta;
    editor_ui.note_view_start = first < 0 ? 0 : first > last ? last : first;
    editor_ui.note_scroll_lock = 1;
}
static void u15_note_editor_touch(int x, int y) {
    note15_layout(editor_ui.edit);
    int row = editor_ui.note_view_start + (y - 54) / 29;
    if (row < 0)
        row = 0;
    if (row >= note_rows)
        row = note_rows - 1;
    const char *p = editor_ui.edit + note_offsets[row], *end = editor_ui.edit + note_offsets[row + 1];
    int px = editor_ui.keyboard && editor_ui.keyboard_target != 30 ? 28 : 48;
    while (p < end && *p != '\n') {
        const char *q = p;
        unsigned c = glyph(&q);
        int w = glyph_advance(ui_font(UI_BODY), c);
        if (x < px + w / 2)
            break;
        px += w;
        p = q;
    }
    editor_ui.note_cursor = p - editor_ui.edit;
}
static void u15_export_photo(void);
static void u15_note_overlay(void) {
    ink_load();
    int height = editor_ui.keyboard ? 448 : 688;
    static unsigned char adjacent[600*688];
    static int cached_note=-1,cached_page=-1,cached_live_page=-1;
    if(cached_live_page!=ink_page){cached_note=cached_page=-1;cached_live_page=ink_page;}
    int offset = (editor_ui.note_view_start - extra_page * 23) * 29;
    for (int y=0;y<height;y++) {
        int source_y=y+offset, page=extra_page;
        while(source_y<0){source_y+=667;page--;}
        while(editor_ui.keyboard && source_y>=667){source_y-=667;page++;}
        if(page<0)continue;
        const unsigned char *pixels=ink_pixels;
        if(page!=ink_page) {
            if(cached_note!=note_selected || cached_page!=page) {
                memset(adjacent,255,sizeof(adjacent));
                char path[2048];snprintf(path,sizeof(path),"%s/notes/%d-page-%d.ink",data_root,note_selected,page);
                FILE *f=fopen(path,"rb");if(f){if(fread(adjacent,1,sizeof(adjacent),f)!=sizeof(adjacent))memset(adjacent,255,sizeof(adjacent));fclose(f);}
                cached_note=note_selected;cached_page=page;
            }
            pixels=adjacent;
        }
        for(int x=0;x<600;x++) {
            unsigned char v=pixels[source_y*600+x];
            if(v<ui_pixels[(y+54)*600+x])ui_pixels[(y+54)*600+x]=v;
        }
    }
}
static void ink_stamp(float x, float y) {
    y -= 54;
    int radius = ink_eraser ? 14 : ink_width;
    float outer = radius + .5f, inner = radius - .5f;
    for (int yy=(int)floorf(y-outer); yy<=y+outer; yy++)
        for (int xx=(int)floorf(x-outer); xx<=x+outer; xx++) {
            if ((unsigned)xx >= 600 || (unsigned)yy >= 688) continue;
            float dx=xx-x, dy=yy-y, distance=dx*dx+dy*dy;
            if (distance >= outer*outer) continue;
            float alpha = distance <= inner*inner ? 1 : outer-sqrtf(distance);
            unsigned char *pixel = &ink_pixels[yy*600+xx];
            int edge = (int)(255*(1-alpha)+.5f);
            if (ink_eraser) {
                if (*pixel < 255-edge) *pixel = 255-edge;
            } else if (*pixel > edge) {
                *pixel = edge;
            }
        }
    ink_dirty = 1;
}
static void ink_line(int x, int y) {
    int dx = x - ink_last_x, dy = y - ink_last_y, n = abs(dx) > abs(dy) ? abs(dx) : abs(dy);
    if (!n)
        n = 1;
    for (int i = 0; i <= n; i++)
        ink_stamp(ink_last_x + dx * (float)i / n, ink_last_y + dy * (float)i / n);
    ink_last_x = x;
    ink_last_y = y;
}
