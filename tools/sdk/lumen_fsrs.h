#ifndef LUMEN_FSRS_H
#define LUMEN_FSRS_H
#include <stdint.h>
typedef struct {
    double stability, difficulty;
    int64_t last, due;
    unsigned reviews, lapses;
} MemoryCard;

#include <math.h>
#include <string.h>
/* FSRS-6 default model, equations and weights from FSRS4Anki v6.1.1.
 * See FSRS-LICENSE.txt. No fitted personal weights or random fuzz. */
static const double fsrs_weights[] = {.212,   1.2931, 2.3065, 8.2956, 6.4133, .8334, 3.0194,
                           .001,   1.8722, .1666,  .796,   1.4835, .0614, .2629,
                           1.6483, .6014,  1.8729, .5425,  .0912,  .0658, .1542};
static double bound(double x, double a, double b) { return fmin(b, fmax(a, x)); }
static double init_d(int g) { return bound(fsrs_weights[4] - exp(fsrs_weights[5] * (g - 1)) + 1, 1, 10); }
static double fsrs_retrievability(const MemoryCard *c, int64_t now) {
    if (!c->reviews || c->stability <= 0)
        return 0;
    double t = fmax(0, (now - c->last) / 86400.);
    return pow(1 + (pow(.9, -1 / fsrs_weights[20]) - 1) * t / c->stability, -fsrs_weights[20]);
}
static void fsrs_review(MemoryCard *c, int g, int64_t now) {
    if (g < 1 || g > 4)
        return;
    double s, d, t = fmax(0, (now - c->last) / 86400.);
    if (!c->reviews) {
        s = fsrs_weights[g - 1];
        d = init_d(g);
    } else {
        double old = fmax(.01, c->stability), od = bound(c->difficulty, 1, 10),
               r = fsrs_retrievability(c, now);
        d = bound(fsrs_weights[7] * init_d(4) + (1 - fsrs_weights[7]) * (od - fsrs_weights[6] * (g - 3) * (10 - od) / 9), 1, 10);
        if (t < 1) {
            double f = exp(fsrs_weights[17] * (g - 3 + fsrs_weights[18])) * pow(old, -fsrs_weights[19]);
            if (g >= 3)
                f = fmax(1, f);
            s = old * f;
        } else if (g == 1)
            s = fmin(fsrs_weights[11] * pow(od, -fsrs_weights[12]) * (pow(old + 1, fsrs_weights[13]) - 1) * exp((1 - r) * fsrs_weights[14]),
                     old / exp(fsrs_weights[17] * fsrs_weights[18]));
        else
            s = old * (1 + exp(fsrs_weights[8]) * (11 - od) * pow(old, -fsrs_weights[9]) * (exp((1 - r) * fsrs_weights[10]) - 1) *
                               (g == 2 ? fsrs_weights[15] : 1) * (g == 4 ? fsrs_weights[16] : 1));
    }
    c->stability = bound(s, .01, 36500);
    c->difficulty = d;
    c->last = now;
    c->reviews++;
    if (g == 1)
        c->lapses++;
    int64_t delay = g == 1                      ? 60
                    : g == 2 && c->reviews == 1 ? 600
                                                : (int64_t)round(bound(s, 1, 36500)) * 86400;
    c->due = now + delay;
}
#endif
