#ifndef LUMEN_CALC_H
#define LUMEN_CALC_H
int calc_evaluate(const char *expression, int degrees, double answer, double memory, double *result,
                  char *error, int cap);
int calc_evaluate_xy(const char *, int, double, double, double, double, double *, char *, int);
#endif
