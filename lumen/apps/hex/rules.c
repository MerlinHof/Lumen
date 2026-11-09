#include "rules.h"
#include <string.h>
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
