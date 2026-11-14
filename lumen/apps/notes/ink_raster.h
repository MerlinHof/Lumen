#ifndef LUMEN_NOTES_INK_RASTER_H
#define LUMEN_NOTES_INK_RASTER_H

#include <math.h>
#include <stdlib.h>

enum { NOTE_INK_WIDTH = 600, NOTE_INK_HEIGHT = 688, NOTE_INK_TOP = 54 };
typedef struct {
    unsigned char *pixels;
    int last_x, last_y;
    int width, erasing;
} NoteInkBrush;

static void note_ink_stamp(NoteInkBrush *brush, float x, float y) {
    y -= NOTE_INK_TOP;
    int radius = brush->erasing ? 14 : brush->width;
    float outer = radius + .5f, inner = radius - .5f;
    for (int row = (int)floorf(y - outer); row <= y + outer; row++) {
        if ((unsigned)row >= NOTE_INK_HEIGHT)
            continue;
        for (int col = (int)floorf(x - outer); col <= x + outer; col++) {
            if ((unsigned)col >= NOTE_INK_WIDTH)
                continue;
            float dx = col - x, dy = row - y;
            float distance = dx * dx + dy * dy;
            if (distance >= outer * outer)
                continue;
            float opacity = distance <= inner * inner ? 1 : outer - sqrtf(distance);
            unsigned char *pixel = brush->pixels + row * NOTE_INK_WIDTH + col;
            int edge = (int)(255 * (1 - opacity) + .5f);
            if (brush->erasing) {
                if (*pixel < 255 - edge)
                    *pixel = 255 - edge;
            } else if (*pixel > edge) {
                *pixel = edge;
            }
        }
    }
}
static inline void note_ink_begin(NoteInkBrush *brush, int x, int y) {
    brush->last_x = x;
    brush->last_y = y;
    note_ink_stamp(brush, x, y);
}
static inline void note_ink_extend(NoteInkBrush *brush, int x, int y) {
    int dx = x - brush->last_x, dy = y - brush->last_y;
    int steps = abs(dx) > abs(dy) ? abs(dx) : abs(dy);
    if (!steps)
        steps = 1;
    for (int i = 0; i <= steps; i++)
        note_ink_stamp(brush, brush->last_x + dx * (float)i / steps,
            brush->last_y + dy * (float)i / steps);
    brush->last_x = x;
    brush->last_y = y;
}
#endif
