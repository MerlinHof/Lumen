#include "api.h"
#include <math.h>
#include <string.h>
/* FSRS-6 default model, equations and weights from FSRS4Anki v6.1.1.
 * See FSRS-LICENSE.txt. No fitted personal weights or random fuzz. */
static const double w[] = {.212,   1.2931, 2.3065, 8.2956, 6.4133, .8334, 3.0194,
                           .001,   1.8722, .1666,  .796,   1.4835, .0614, .2629,
                           1.6483, .6014,  1.8729, .5425,  .0912,  .0658, .1542};
static double bound(double x, double a, double b) { return fmin(b, fmax(a, x)); }
static double init_d(int g) { return bound(w[4] - exp(w[5] * (g - 1)) + 1, 1, 10); }
double fsrs_retrievability(const MemoryCard *c, int64_t now) {
    if (!c->reviews || c->stability <= 0)
        return 0;
    double t = fmax(0, (now - c->last) / 86400.);
    return pow(1 + (pow(.9, -1 / w[20]) - 1) * t / c->stability, -w[20]);
}
void fsrs_review(MemoryCard *c, int g, int64_t now) {
    if (g < 1 || g > 4)
        return;
    double s, d, t = fmax(0, (now - c->last) / 86400.);
    if (!c->reviews) {
        s = w[g - 1];
        d = init_d(g);
    } else {
        double old = fmax(.01, c->stability), od = bound(c->difficulty, 1, 10),
               r = fsrs_retrievability(c, now);
        d = bound(w[7] * init_d(4) + (1 - w[7]) * (od - w[6] * (g - 3) * (10 - od) / 9), 1, 10);
        if (t < 1) {
            double f = exp(w[17] * (g - 3 + w[18])) * pow(old, -w[19]);
            if (g >= 3)
                f = fmax(1, f);
            s = old * f;
        } else if (g == 1)
            s = fmin(w[11] * pow(od, -w[12]) * (pow(old + 1, w[13]) - 1) * exp((1 - r) * w[14]),
                     old / exp(w[17] * w[18]));
        else
            s = old * (1 + exp(w[8]) * (11 - od) * pow(old, -w[9]) * (exp((1 - r) * w[10]) - 1) *
                               (g == 2 ? w[15] : 1) * (g == 4 ? w[16] : 1));
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
void wordle_score(const char *s, const char *g, unsigned char r[5]) {
    int used[5] = {0};
    memset(r, 0, 5);
    for (int i = 0; i < 5; i++)
        if (s[i] == g[i]) {
            r[i] = 2;
            used[i] = 1;
        }
    for (int i = 0; i < 5; i++)
        if (!r[i])
            for (int j = 0; j < 5; j++)
                if (!used[j] && s[j] == g[i]) {
                    r[i] = 1;
                    used[j] = 1;
                    break;
                }
}
int hex_winner(const unsigned char *b, int n) {
    if (n < 1 || n > 13)
        return 0;
    for (int p = 1; p <= 2; p++) {
        int q[169], head = 0, tail = 0;
        unsigned char seen[169] = {0};
        for (int i = 0; i < n; i++) {
            int k = p == 1 ? i : n * i;
            if (b[k] == p) {
                q[tail++] = k;
                seen[k] = 1;
            }
        }
        while (head < tail) {
            int k = q[head++], r = k / n, c = k % n;
            if ((p == 1 ? r : c) == n - 1)
                return p;
            int dr[] = {-1, -1, 0, 0, 1, 1}, dc[] = {0, 1, -1, 1, -1, 0};
            for (int j = 0; j < 6; j++) {
                int y = r + dr[j], x = c + dc[j], v = y * n + x;
                if (y >= 0 && y < n && x >= 0 && x < n && !seen[v] && b[v] == p) {
                    seen[v] = 1;
                    q[tail++] = v;
                }
            }
        }
    }
    return 0;
}
