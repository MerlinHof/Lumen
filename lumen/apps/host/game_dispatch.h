/* App-owned module; assembled by apps/host/compose.h. */
static void new_apps_draw(void) {
    if (ui_route() >= 70) {
        v1_draw();
        return;
    }
    switch (ui_route()) {
    case 60:
        sensor_graph_screen();
        break;
    case 61:
        calculator_graph();
        break;
    case 62:
        graph_range_screen();
        break;
    case 63:
        app_tiles();
        break;
    case 64:
        app_solitaire();
        break;
    case 65:
        app_sudoku();
        break;
    case 66:
        app_focus();
        break;
    case 67:
        app_newspaper();
        break;
    case 68:
        app_news_settings();
        break;
    case 69:
        app_news_source();
        break;
    }
}
static int new_apps_commit(int target, const char *text) {
    if (target >= 100)
        return v1_commit(target, text);
    if (target == 80) {
        snprintf(calc_expr, sizeof(calc_expr), "%s", text);
        calc_cursor = strlen(calc_expr);
        calc_preview();
        return 1;
    }
    if (target >= 81 && target <= 84) {
        char *end;
        double v = strtod(text, &end);
        int i = target - 81;
        if (end == text || *end || !isfinite(v) || fabs(v) > 1e12) {
            ui_message("Ausdruck oder Wertebereich prüfen.");
            return 1;
        }
        double copy[4];
        memcpy(copy, plot_bounds, sizeof(copy));
        copy[i] = v;
        if (copy[0] >= copy[1] || copy[2] >= copy[3])
            ui_message("Minimum muss kleiner als Maximum sein.");
        else
            memcpy(plot_bounds, copy, sizeof(copy));
        return 1;
    }
    return 0;
}
static int new_apps_input(const Input *in) {
    if (ui_route() >= 70)
        return v1_input(in);
    int x = in->x, y = in->y;
    if (ui_route() == 63 && in->kind == INPUT_SWIPE) {
        int dir = abs(in->dx) > abs(in->dy) ? (in->dx < 0 ? 0 : 1) : (in->dy < 0 ? 2 : 3);
        if (tiles_move(&tile_game, dir)) {
            new_dirty |= 1;
            new_apps_save();
        }
        return 1;
    }
    if (ui_route() == 67 && in->kind == INPUT_SWIPE) {
        int total = newspaper.count + 2;
        if (in->dx < 0 && news_page + 1 < total)
            news_page++;
        else if (in->dx > 0 && news_page)
            news_page--;
        return 1;
    }
    if (in->kind != INPUT_TAP)
        return 2;
    if (ui_route() == 60) {
        if (y >= 716 && y < 764) {
            ha_plot_hours = x < 210 ? 1 : x < 390 ? 6 : 24;
        }
        return 1;
    }
    if (ui_route() == 61) {
        if (y >= 128 && y < 178)
            open_keyboard(80, "y = f(x)", calc_expr);
        else if (y >= 220 && y < 647 && x >= 66 && x < 558)
            plot_trace = x - 66 + 1;
        else if (y >= 712) {
            if (x < 178)
                extra_open(62);
            else if (x >= 416)
                graph_auto_y();
            else {
                double f = x < 300 ? 2 : .5, cx = (plot_bounds[0] + plot_bounds[1]) / 2,
                       cy = (plot_bounds[2] + plot_bounds[3]) / 2,
                       dx = (plot_bounds[1] - plot_bounds[0]) * f / 2,
                       dy = (plot_bounds[3] - plot_bounds[2]) * f / 2;
                if (dx > 1e-9 && dx < 1e12 && dy > 1e-9 && dy < 1e12) {
                    plot_bounds[0] = cx - dx;
                    plot_bounds[1] = cx + dx;
                    plot_bounds[2] = cy - dy;
                    plot_bounds[3] = cy + dy;
                }
            }
        }
        return 1;
    }
    if (ui_route() == 62) {
        if (y >= 165 && y < 549) {
            int i = (y - 165) / 108;
            if (i < 4) {
                char b[80];
                snprintf(b, sizeof(b), "%.10g", plot_bounds[i]);
                open_keyboard(81 + i, "Graph-Bereich", b);
            }
        } else if (y >= 650 && y < 706)
            extra_return(61);
        return 1;
    }
    if (ui_route() == 63) {
        new_dirty |= 1;
        if (y >= 74 && y < 118) {
            if (x >= 516)
                tiles_new(&tile_game, pro_random());
            else if (x >= 460 && tile_game.can_undo) {
                tile_game.state = tile_game.undo;
                tile_game.can_undo = 0;
            }
        } else if (y >= 716 && y < 772 && x >= 28) {
            int k = (x - 28) / 140;
            int d[] = {0, 2, 3, 1};
            if (k < 4)
                tiles_move(&tile_game, d[k]);
        }
        new_apps_save();
        return 1;
    }
    if (ui_route() == 64) {
        new_dirty |= 2;
        if (y >= 74 && y < 118) {
            if (x >= 516)
                patience_new(&solitaire, pro_random());
            else if (x >= 460)
                patience_auto(&solitaire);
            else if (x >= 405)
                patience_undo(&solitaire);
            pat_source = -99;
        } else if (y >= 132 && y < 228) {
            if (x >= 21 && x < 93) {
                patience_draw(&solitaire);
                pat_source = -99;
            } else if (x >= 101 && x < 173 && solitaire.state.nw) {
                pat_source = -1;
                pat_index = 0;
            } else if (x >= 261 && x < 573) {
                int to = 7 + (x - 261) / 80;
                if (pat_source != -99 && patience_move(&solitaire, pat_source, pat_index, to))
                    pat_source = -99;
                else if (solitaire.state.foundation[to - 7]) {
                    pat_source = to;
                    pat_index = 0;
                }
            }
        } else if (y >= 248 && y < 710 && x >= 21 && x < 573) {
            int c = (x - 21) / 80;
            if (c < 7) {
                if (pat_source != -99 && patience_move(&solitaire, pat_source, pat_index, c))
                    pat_source = -99;
                else if (solitaire.state.count[c]) {
                    int idx = (y - 248) / column_gap(c);
                    if (idx >= solitaire.state.count[c])
                        idx = solitaire.state.count[c] - 1;
                    if (idx >= solitaire.state.hidden[c]) {
                        pat_source = c;
                        pat_index = idx;
                    }
                }
            }
        }
        new_apps_save();
        return 1;
    }
    if (ui_route() == 65) {
        new_dirty |= 4;
        if (y >= 74 && y < 118 && x >= 516) {
            sudoku_new(&sudoku, pro_random(), sudoku.difficulty);
            sudoku_selected = -1;
        } else if (y >= 116 && y < 160) {
            if (x < 170)
                sudoku_pencil ^= 1;
            else if (x < 230)
                sudoku_undo(&sudoku);
            else if (x < 300)
                sudoku_put(&sudoku, sudoku_selected, 0, 0);
            else if (x >= 360)
                sudoku_hint(&sudoku, sudoku_selected);
        } else if (x >= 39 && x < 561 && y >= 170 && y < 692)
            sudoku_selected = (y - 170) / 58 * 9 + (x - 39) / 58;
        else if (y >= 725 && y < 777 && x >= 28 && x < 573) {
            int v = (x - 28) / 61 + 1;
            if (v <= 9)
                sudoku_put(&sudoku, sudoku_selected, v, sudoku_pencil);
        }
        new_apps_save();
        return 1;
    }
    if (ui_route() == 66) {
        return polish_brain_input(in);
    }
    if (ui_route() == 67) {
        return polish_news_input(in);
    }
    if (ui_route() == 68) {
        if (y >= 142 && y < 142 + 6 * 74 && x >= 460) {
            int i = (y - 142) / 74;
            if (i < 6) {
                news_topics ^= 1u << i;
                if (!(news_topics & 31))
                    news_topics |= 1;
                news_save_settings();
            }
        } else if (y >= 706 && y < 758)
            extra_return(67);
        return 1;
    }
    return 1;
}
