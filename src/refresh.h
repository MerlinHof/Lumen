#ifndef LUMEN_REFRESH_H
#define LUMEN_REFRESH_H
#include <stdlib.h>
/* Compare block luminance, not just the bounding rectangle: two distant tiny
 * changes and ordinary text page turns must not imply a whole-screen flash.
 * Persistent empty white margins do not dilute a large visible-content change. */
static inline int refresh_coverage(const unsigned char *a, const unsigned char *b, int w, int h) {
    long changed_area = 0, content_area = 0;
    for (int y = 0; y < h; y += 32)
        for (int x = 0; x < w; x += 24) {
            int n = 0, sa = 0, sb = 0, energy = 0;
            for (int j = y; j < y + 32 && j < h; j++)
                for (int i = x; i < x + 24 && i < w; i++) {
                    int u = a[j * w + i], v = b[j * w + i];
                    sa += u;
                    sb += v;
                    energy += abs(u - v);
                    n++;
                }
            if (sa < n * 250 || sb < n * 250)
                content_area += n;
            if (abs(sa - sb) >= n * 12 || energy >= n * 80)
                changed_area += n;
        }
    return content_area ? (int)(100 * changed_area / content_area) : 0;
}
#endif
