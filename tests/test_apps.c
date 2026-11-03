#include "../src/apps.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    assert(calendar_days(2000, 2) == 29 && calendar_days(1900, 2) == 28 &&
           calendar_days(2028, 2) == 29 && calendar_days(2026, 13) == 0);
    for (unsigned seed = 1; seed < 80; seed++) {
        apps_seed(seed);
        Mines m;
        mines_new(&m);
        mines_reveal(&m, 44);
        assert(!m.over && m.open[44] && !mines_adjacent(&m, 44));
        int bombs = 0;
        for (int i = 0; i < 90; i++) {
            bombs += m.bomb[i];
            if (m.bomb[i]) {
                mines_flag(&m, i);
                assert(m.flag[i]);
                mines_flag(&m, i);
                assert(!m.flag[i]);
            }
        }
        assert(bombs == 12);
        for (int i = 0; i < 90; i++)
            if (!m.bomb[i])
                mines_reveal(&m, i);
        assert(m.won && m.over);
    }
    Hangman h;
    hangman_new(&h, 1);
    strcpy(h.word, "BOOK");
    hangman_guess(&h, 'A' - 'A');
    hangman_guess(&h, 'A' - 'A');
    assert(h.misses == 1);
    for (const char *p = h.word; *p; p++)
        hangman_guess(&h, *p - 'A');
    assert(h.won);
    hangman_new(&h, 0);
    strcpy(h.word, "ZZZ");
    for (int i = 0; i < 6; i++)
        hangman_guess(&h, i);
    assert(h.over && !h.won && h.misses == 6);
    for (int p = 0; p < 7; p++)
        for (int r = 0; r < 4; r++) {
            int n = 0;
            for (int y = 0; y < 4; y++)
                for (int x = 0; x < 4; x++)
                    n += blocks_cell(p, r, x, y);
            assert(n == 4);
        }
    Blocks b;
    blocks_new(&b);
    int initial = b.y;
    blocks_tick(&b);
    assert(b.y == initial);
    b.paused = 0;
    b.piece = 0;
    b.rotation = 0;
    b.x = 3;
    b.y = 0;
    memset(b.board, 0, sizeof(b.board));
    for (int x = 0; x < 10; x++)
        b.board[15][x] = x < 3 || x > 6;
    assert(blocks_landing_y(&b) == 14);
    blocks_drop(&b);
    assert(b.lines == 1 && b.score >= 100);
    for (int x = 0; x < 10; x++)
        assert(!b.board[15][x]);
    for (int i = 0; i < 30; i++)
        blocks_move(&b, -1);
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++)
            if (blocks_cell(b.piece, b.rotation, x, y))
                assert(b.x + x >= 0);
    for (int i = 0; i < 10000; i++) {
        if (b.over)
            blocks_new(&b);
        b.paused = 0;
        blocks_rotate(&b);
        blocks_move(&b, (i % 3) - 1);
        blocks_drop(&b);
    }
    Snake s;
    snake_new(&s);
    int head = s.cells[0];
    snake_tick(&s);
    assert(s.cells[0] == head);
    s.paused = 0;
    snake_direction(&s, -1, 0);
    assert(s.next_dx == 1);
    s.food = head + 1;
    snake_tick(&s);
    assert(s.length == 5 && s.score == 1 && !s.over);
    for (int i = 0; i < s.length; i++)
        assert(s.food != s.cells[i]);
    for (int i = 0; i < 20 && !s.over; i++)
        snake_tick(&s);
    assert(s.over);
    snake_new(&s);
    s.paused = 0;
    s.length = 4;
    s.cells[0] = 19;
    s.cells[1] = 20;
    s.cells[2] = 2;
    s.cells[3] = 1;
    s.dx = s.next_dx = -1;
    s.dy = s.next_dy = 0;
    s.food = 100;
    snake_direction(&s, 0, -1);
    snake_tick(&s);
    assert(!s.over && s.cells[0] == 1);
    puts("PASS: calendar leap years, safe first mine reveal and flood/win, flags, word guesses, "
         "rotations, line clearing, collisions, snake growth, reverse rejection, tail vacancy, "
         "10000 block drops.");
}
