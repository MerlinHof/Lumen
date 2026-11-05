#ifndef LUMEN_UI_PAINT_H
#define LUMEN_UI_PAINT_H
#include "icon_ids.h"
#include <stddef.h>
#include <stdint.h>

typedef enum { PAINT_BODY, PAINT_HEADING, PAINT_DISPLAY, PAINT_SECONDARY } PaintText;
void ui_paint_heading(const char *text, int actions);
void ui_paint_icon_button(int x, int y, int icon_id);
void ui_paint_hitbox(int x, int y, int w, int h);
void ui_paint_text(int x, int y, int w, int h, const char *text, PaintText role);
void ui_paint_line(int x0, int y0, int x1, int y1, int weight, int gray);
void ui_paint_ellipse(int x, int y, int rx, int ry, int gray);
void ui_paint_ring(int x, int y, int radius, int weight);
void ui_paint_image(int x, int y, int w, int h, const unsigned char *gray);
int64_t ui_clock_millis(void);
#endif
