#ifndef LUMEN_SIGNAL_H
#define LUMEN_SIGNAL_H
#define SIGNAL_W 300
#define SIGNAL_H 250
typedef struct { float x,z,yaw; unsigned lights,steps; int won; } SignalGame;
void signal_new(SignalGame *g);
int signal_move(SignalGame *g,int forward,int turn);
int signal_activate(SignalGame *g);
void signal_render(const SignalGame *g,unsigned char *pixels);
int signal_near(const SignalGame *g);
#endif
