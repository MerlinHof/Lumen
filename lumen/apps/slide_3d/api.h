#ifndef LUMEN_SLIDE3D_H
#define LUMEN_SLIDE3D_H
#define SIGNAL_W 600
#define SIGNAL_H 500
typedef struct { int tiles[16], empty, steps; } SignalGame;
void signal_new(SignalGame *, unsigned seed, int moves);
int signal_slide(SignalGame *, int index);
int signal_won(const SignalGame *);
void signal_project(int index, int *x, int *y);
void signal_render(const SignalGame *, unsigned char *);
void slide_3d_register(void);
#endif
