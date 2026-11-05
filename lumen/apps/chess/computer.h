/* App-owned module; assembled by apps/host/compose.h. */
static int chess_eval(const ChessPosition *p) {
    static const int val[] = {0, 100, 320, 330, 500, 900, 20000};
    int n = 0;
    for (int i = 0; i < 64; i++) {
        int v = p->b[i];
        if (v) {
            int center = 7 - abs(i % 8 * 2 - 7) / 2 - abs(i / 8 * 2 - 7) / 2;
            n += side(v) * (val[abs(v)] + (abs(v) == 1  ? (v > 0 ? 6 - i / 8 : i / 8 - 1) * 5
                                           : abs(v) < 4 ? center * 5
                                                        : 0));
        }
    }
    return n * p->turn;
}
static int chess_search(const ChessPosition *p, int depth, int alpha, int beta) {
    if (!depth || ++chess_nodes > 3500)
        return chess_eval(p);
    int best = -30000, moves = 0;
    for (int captures = 1; captures >= 0; captures--)
        for (int f = 0; f < 64; f++)
            if (side(p->b[f]) == p->turn)
                for (int t = 0; t < 64; t++) {
                    if ((p->b[t] != 0) != captures || !chess_pseudo(p, f, t) ||
                        !chess_legal(p, f, t))
                        continue;
                    moves++;
                    ChessPosition q = *p;
                    chess_apply(&q, f, t, 5);
                    int score = -chess_search(&q, depth - 1, -beta, -alpha);
                    if (score > best)
                        best = score;
                    if (score > alpha)
                        alpha = score;
                    if (alpha >= beta)
                        return alpha;
                }
    return moves ? best : chess_check(p, p->turn) ? -29000 - depth : 0;
}
static int v1_chess_tick(void) {
    if (chess_worker > 0) {
        int status;
        pid_t done = waitpid(chess_worker, &status, WNOHANG);
        if (done == 0)
            return 0;
        chess_worker = -1;
        if (done > 0 && WIFEXITED(status) && !WEXITSTATUS(status) && screen == 48 &&
            chess_computer && !memcmp(&chess, &chess_job, sizeof(chess))) {
            int move[2];
            if (load_blob("chess-ai-result.bin", move, sizeof(move)) &&
                chess_legal(&chess, move[0], move[1])) {
                if (chess_history < 256)
                    chess_undo[chess_history++] = chess;
                chess_apply(&chess, move[0], move[1], 5);
                chess_store();
                return 1;
            }
        }
    }
    if (screen != 48 || !chess_computer || chess.turn != -1 || chess_drawn() || !chess_moves())
        return 0;
    chess_job = chess;
    chess_worker = fork();
    if (chess_worker == 0) {
        chess_nodes = 0;
        int move[2] = {-1, -1}, best = -40000;
        for (int f = 0; f < 64; f++)
            if (side(chess.b[f]) == chess.turn)
                for (int t = 0; t < 64; t++)
                    if (chess_pseudo(&chess, f, t) && chess_legal(&chess, f, t)) {
                        ChessPosition q = chess;
                        chess_apply(&q, f, t, 5);
                        int score = -chess_search(&q, 2, -30000, 30000);
                        if (score > best) {
                            best = score;
                            move[0] = f;
                            move[1] = t;
                        }
                    }
        int ok = save_blob("chess-ai-result.bin", move, sizeof(move));
        _exit(ok ? 0 : 1);
    }
    return 1;
}
