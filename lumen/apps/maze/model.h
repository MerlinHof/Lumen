/* App-owned module; assembled by apps/host/compose.h. */
static void maze_new17(void) {
    memset(maze17, 1, sizeof(maze17));
    memset(maze_trace17,0,sizeof(maze_trace17));
    maze_trace17[34]=1;
    int stack[300], top = 0;
    stack[top++] = 34;
    maze17[34] = 0;
    while (top) {
        int at = stack[top - 1], x = at % 33, y = at / 33, choices[4], n = 0;
        const int dx[] = {2, -2, 0, 0}, dy[] = {0, 0, 2, -2};
        for (int d = 0; d < 4; d++) {
            int xx = x + dx[d], yy = y + dy[d];
            if (xx > 0 && xx < 32 && yy > 0 && yy < 34 && maze17[yy * 33 + xx])
                choices[n++] = d;
        }
        if (!n) {
            top--;
            continue;
        }
        int d = choices[random16() % n], next = (y + dy[d]) * 33 + x + dx[d];
        maze17[(at + next) / 2] = maze17[next] = 0;
        stack[top++] = next;
    }
    maze_x17 = maze_y17 = 1;
    maze_moves17 = maze_win17 = 0;
    maze_ready17 = 1;
}
static void maze_step17(int dx, int dy, int steps) {
    while (steps-- && !maze_win17) {
        int x = maze_x17 + dx, y = maze_y17 + dy;
        if (x < 0 || x >= 33 || y < 0 || y >= 35 || maze17[y * 33 + x])
            break;
        maze_x17 = x;
        maze_y17 = y;
        maze_trace17[y*33+x]=1;
        maze_moves17++;
        if (x == 31 && y == 33) {
            maze_win17 = 1;
            ui_message("Ausgang gefunden!");
        }
    }
}
