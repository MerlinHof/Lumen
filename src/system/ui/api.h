#ifndef LUMEN_UI_H
#define LUMEN_UI_H
#include "../device.h"
extern unsigned char ui_pixels[WIDTH * HEIGHT];
void ui_init(void);
void ui_loading(const char *message);
void ui_draw(void);
void ui_feedback_draw(void);
int ui_poll_interval(void);
int ui_batch_input(void);
int ui_input(const Input *input);
int ui_open(int index);
void ui_message(const char *message);
void ui_set_screen(int screen);
void ui_save(void);
void ui_close(void);
int ui_power_request(void);
void ui_power_screen(int restart);
int ui_full_refresh(void);
#endif
