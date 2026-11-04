/* App-owned state; stable on-disk schemas are intentionally preserved. */
static unsigned char nono_solution[25], nono_cells[25], hex_board[121], hex_previous[121];
static int nono_ready, nono_mark, hex_turn = 1, hex_win, hex_swap, hex_has_undo;
static int lang_pair, lang_index, lang_reveal, lang_correct, lang_total;
