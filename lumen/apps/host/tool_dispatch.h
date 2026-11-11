/* App-owned module; assembled by apps/host/compose.h. */
static void extra_draw(void) {
    switch (ui_route()) {
    case 51:
        heading("Bildquelle");
        wrapped_raw(30, 150, discover_lookup_image_name(), ui_font(UI_PRIMARY), 540, 5);
        wrapped(30, 330,
                "Freies Artikelbild von Wikimedia. Die genaue Lizenz und der Urheber stehen auf "
                "der Dateiseite:",
                ui_font(UI_BODY), 540, 4);
        char source[1024];
        snprintf(source, sizeof(source), "https://commons.wikimedia.org/wiki/File:%s",
                 discover_lookup_image_name());
        wrapped_raw(30, 470, source, ui_font(UI_SECONDARY), 540, 10);
        break;
    case 48:
        app_chess();
        break;
    case 50:
        heading("Neue Partie?");
        wrapped(30, 200, "Aktuelle Schachpartie beenden?", ui_font(UI_TITLE), 540, 4);
        button_h(28, 455, 544, 60, "Neue Partie", 0);
        button_h(28, 540, 544, 60, "Abbrechen", 0);
        break;
    case 27:
        app_calculator();
        break;
    case 28:
        app_timer();
        break;
    case 29:
        app_notes();
        break;
    case 30:
        app_note();
        break;
    case 32:
        app_weather();
        break;
    case 33:
        app_home_assistant();
        break;
    case 34:
        app_ha_setup();
        break;
    case 36:
        app_wiki();
        break;
    case 39:
        app_time_settings();
        break;
    case 40:
        system_screen();
        break;
    case 41:
        app_reset();
        break;
    case 46: {
        heading("Weckzeit");
        button_h(100, 245, 180, 60, "+", 0);
        button_h(320, 245, 180, 60, "+", 0);
        char b[20];
        snprintf(b, sizeof(b), "%02d", alarm_hour);
        centered(100, 320, 180, 80, b, ui_font(UI_DISPLAY));
        snprintf(b, sizeof(b), "%02d", alarm_minute);
        centered(320, 320, 180, 80, b, ui_font(UI_DISPLAY));
        centered(280, 320, 40, 80, ":", ui_font(UI_DISPLAY));
        button_h(100, 415, 180, 60, "−", 0);
        button_h(320, 415, 180, 60, "−", 0);
        button_h(28, 554, 544, 60, "Fertig", 0);
        break;
    }
    case 47: {
        heading("Datum & Uhrzeit");
        const char *names[] = {"Tag", "Monat", "Jahr", "Stunde", "Minute"};
        int values[] = {time_day, time_month, time_year, time_hour, time_minute};
        char b[40];
        for (int i = 0; i < 5; i++) {
            int y = 170 + i * 86;
            drawtext(30, y + 10, names[i], ui_font(UI_BODY), 170);
            snprintf(b, sizeof(b), i == 2 ? "%04d" : "%02d", values[i]);
            centered(200, y, 170, 54, b, ui_font(UI_PRIMARY));
            button_h(390, y, 72, 54, "−", 0);
            button_h(480, y, 92, 54, "+", 0);
        }
        button_h(28, 650, 544, 56, "Fertig", 0);
        break;
    }
    case 42:
        app_calc_history();
        break;
    case 43:
    case 44:
    case 45:
        heading("Löschen?");
        wrapped(30, 180,
                ui_route() == 43   ? "Diese Notiz endgültig löschen?"
                : ui_route() == 45 ? "Diese Tagesnotiz löschen?"
                               : "Dieses Zeichenblatt leeren?",
                ui_font(UI_TITLE), 540, 4);
        button(28, 455, 544, "Löschen", 0);
        button(28, 530, 544, "Abbrechen", 0);
        break;
    }
}
static void extra_return(int target) {
    navigation_rewinds++;
    for (int i = history_count - 1; i >= 0; i--)
        if (history[i] == target) {
            history_count = i;
            break;
        }
    route_to(target);
}
static void extra_commit(int target, const char *text) {
    if(package_ui_commit(target,text))return;
    if (u15_commit(target, text))
        return;
    if (new_apps_commit(target, text))
        return;
    if (target == 61) {
        snprintf(ha_search, sizeof(ha_search), "%s", text);
        ha_page = 0;
        return;
    }

    if (target == 30) {
        if (note_selected < 0) {
            int id = (int)time(NULL);
            while (1) {
                int exists = 0;
                for (int i = 0; i < note_count; i++)
                    if (note_ids[i] == id)
                        exists = 1;
                if (!exists)
                    break;
                id++;
            }
            note_selected = id;
            v1_note_created(id);
        }
        snprintf(note_text, sizeof(note_text), "%s", text);
        char path[80];
        snprintf(path, sizeof(path), "notes/%d.txt", note_selected);
        extra_write_text(path, note_text);
        notes_scan();
        if (ui_route() == 29 && history_count < 64 &&
            (!history_count || history[history_count - 1] != 29))
            history[history_count++] = 29;
        extra_return(30);
        extra_page = 0;
    }  else if (target == 32) {
        if (strpbrk(text, "\r\n\"\\")) {
            ui_message("Ungültiger Token.");
            return;
        }
        snprintf(ha_token, sizeof(ha_token), "%s", text);
        extra_write_text("ha-token.txt", ha_token);
    } else if (target == 33) {
        if (!*text)
            return;
        snprintf(weather_city, sizeof(weather_city), "%s", text);
        extra_write_text("weather-city.txt", weather_city);
        if (!service_start(SERVICE_WEATHER, weather_city, "", ""))
            ui_message("Anfrage läuft bereits.");
    } else if (target == 34) {
        wiki_ui.wiki_article_language = options[OPT_LANGUAGE];
        snprintf(wiki_query, sizeof(wiki_query), "%s", text);
        wiki_page = 0;
        extra_write_text("wiki-query.txt", wiki_query);
        discover_lookup(wiki_query, options[OPT_LANGUAGE]);
    } else if (target == 35) {
        int y, m, d, h, minute;
        char tail;
        if (sscanf(text, "%d-%d-%d %d:%d%c", &y, &m, &d, &h, &minute, &tail) != 5 ||
            !system_set_time(y, m, d, h, minute))
            ui_message("Ungültiges Datum / Uhrzeit.");
        else {
            status_ui.status_time = 0;
            ui_message("Datum und Uhrzeit gesetzt.");
        }
    } else if (target == 36) {
        char *end;
        long n = strtol(text, &end, 10);
        if (!*end && n >= 1 && n <= 1440)
            timer_minutes = (int)n;
        else
            ui_message("1 bis 1440 Minuten eingeben.");
        extra_save();
    } else if (target == 37) {
        int h, m;
        char c;
        if (sscanf(text, "%d:%d%c", &h, &m, &c) == 2 && h >= 0 && h < 24 && m >= 0 && m < 60) {
            alarm_hour = h;
            alarm_minute = m;
        } else
            ui_message("Uhrzeit als HH:MM eingeben.");
        extra_save();
    }
}

#include "../timer/input.h"
static int extra_input(const Input *in) {
    if (ui_route()==60) return sensor_graph_input(in);
    int x = in->x, y = in->y;
    if (ui_route() == 48 || ui_route() == 49 || ui_route() == 50)
        return games_input(in);

    if (in->kind != INPUT_TAP)
        return 2;

    if (ui_route() == 27) {
        if (y >= 74 && y < 118 && x >= 408 && x < 460) {
            if (!*calc_expr)
                snprintf(calc_expr, sizeof(calc_expr), "sin(x)");
            extra_open(61);
            return 1;
        }
        if (y >= 74 && y < 118 && x >= 460 && x < 516) {
            extra_open(42);
            return 1;
        }
        if (y >= 361 && y < 413 && x >= 528) {
            calc_backspace();
            return 1;
        }
        if (y >= 361 && y < 413) {
            int start = calc_input_start();
            int w = 43;
            const char *p = calc_expr + text_boundary(calc_expr, start);
            while (*p) {
                const char *next = p;
                unsigned c = glyph(&next);
                int a = glyph_advance(ui_font(UI_BODY), c);
                if (x < w + a / 2)
                    break;
                w += a;
                p = next;
            }
            calc_cursor = p - calc_expr;
            return 1;
        }
        if (y >= 429 && y < 531 && x >= 28 && x < 572) {
            if ((!calc_alt && x >= 534) || (calc_alt && x < 66)) {
                calc_alt ^= 1;
                return 1;
            }
            int r = (y - 429) / 51, c = (x - calc_function_left()) / calc_function_pitch();
            if (c >= 0 && c < 8 && y < 429 + r * 51 + 44 &&
                x < calc_function_left() + c * calc_function_pitch() + 57) {
                const char *key = (calc_alt ? calc_all1 : calc_all0)[r * 8 + c];
                if (!strcmp(key, "M+")) {
                    double value;
                    char err[80];
                    calc_memory += calc_evaluate(calc_expr, calc_degrees, calc_answer, calc_memory,
                                                 &value, err, sizeof(err))
                                       ? value
                                       : calc_answer;
                } else if (!strcmp(key, "MC"))
                    calc_memory = 0;
                else if (isalpha((unsigned char)key[0]) && strcmp(key, "pi") && strcmp(key, "e") &&
                         strcmp(key, "ans") && strcmp(key, "mem") && strcmp(key, "x") &&
                         strcmp(key, "y")) {
                    calc_insert(key);
                    calc_insert("(");
                } else
                    calc_insert(key);
            }
            return 1;
        }
        if (y >= 540 && y < 782) {
            if (x >= 460) {
                if (y >= 664)
                    calc_run();
                else if (y < 596) {
                    if (!*calc_expr)
                        calc_insert("-");
                    else if (strlen(calc_expr) < sizeof(calc_expr) - 4) {
                        char opposite[1024];
                        snprintf(opposite, sizeof(opposite), "-(%s)", calc_expr);
                        snprintf(calc_expr, sizeof(calc_expr), "%s", opposite);
                        calc_cursor = strlen(calc_expr);
                        calc_preview();
                    }
                } else if (y >= 602 && y < 658) {
                    calc_expr[0] = calc_result[0] = calc_error[0] = 0;
                    calc_cursor = 0;
                }
            } else if (x >= 28 && x < 452) {
                int c = (x - 28) / 108, r = (y - 540) / 62;
                if (c < 4 && r < 4 && x < 28 + c * 108 + 100 && y < 540 + r * 62 + 56)
                    calc_insert(calc_digits[r * 4 + c]);
            }
            return 1;
        }
        return 1;
    }
    if (ui_route() == 42) {
        if (y >= 145 && y < 657) {
            int i = calc_history_count - 1 - (extra_page * 8 + (y - 145) / 64);
            if (i >= 0) {
                snprintf(calc_expr, sizeof(calc_expr), "%s", calc_history[i]);
                calc_cursor = strlen(calc_expr);
                extra_return(27);
                calc_preview();
                calc_error[0] = 0;
            }
        } else if (y >= 679 && y < 723) {
            if (x < 300 && extra_page)
                extra_page--;
            else if (x >= 300 && (extra_page + 1) * 8 < calc_history_count)
                extra_page++;
        }
        return 1;
    }
    if (ui_route() == 28 || ui_route() == 46) return clock_input(x, y);
    if (ui_route() == 29)
        return v1_notes_input(in);
    if (ui_route() == 30) {
        if (y >= 54 && y < 98 && x >= 524) {
            reader_markdown17("Notizen",note_text,30);
            return 1;
        }
        if (y >= 54 && y < 740) {
            open_keyboard(30, "Notiz", note_text);
            editor_ui.note_cursor = strlen(editor_ui.edit);
            editor_ui.note_view_start = extra_page * 23;
            note_editor_touch(x, y);
        } else if (y >= 748) {
            if (x < 300 && extra_page)
                extra_page--;
            else if (x >= 300) {
                const char *p = note_text;
                for (int i = 0; i < (extra_page + 1) * 23 && *p; i++)
                    p = note_next_line(p, 536);
                if (*p)
                    extra_page++;
            }
        }
        return 1;
    }

    if (ui_route() == 32) {
        if (y >= 74 && y < 118) {
            if (x >= 516) {
                if (!service_start(SERVICE_WEATHER, weather_city, "", ""))
                    ui_message("Anfrage läuft bereits.");
            } else if (x >= 460)
                open_keyboard(33, "Wetter: Ort", weather_city);
        }
        return 1;
    }
    if (ui_route() == 33) {
        if (y >= 74 && y < 118) {
            if (x >= 516) {
                if (!ha_back()) {
                    ha_select = 1;
                    ha_page = 0;
                }
            } else if (x >= 456)
                extra_open(34);
        } else if ((!*ha_url || !*ha_token) && y >= 420 && y < 468)
            extra_open(34);
        else if (ha_select && y >= 187 && y < 235) {
            open_keyboard(61, "Geräte suchen", ha_search);
        } else if ((ha_select ? y >= 250 && y < 658
                              : y >= 148 && y < 708 && ((y - 148) % 140 < 126) &&
                                    (x < 290 || x >= 310)) &&
                   x >= 28 && x < 572) {
            int v[512], n = ha_visible(v);
            int cell = ha_select ? (y - 250) / 68 : ((y - 148) / 140) * 2 + (x >= 310);
            int row = ha_page * (ha_select ? 6 : 8) + cell;
            if (row < n) {
                HAEntity *e = &ha_entities[v[row]];
                if (ha_select) {
                    int fav = ha_favorite(e->id);
                    if (fav >= 0) {
                        memmove(ha_favorites + fav, ha_favorites + fav + 1,
                                (ha_favorite_count - fav - 1) * sizeof(ha_favorites[0]));
                        ha_favorite_count--;
                    } else if (ha_favorite_count < 128)
                        snprintf(ha_favorites[ha_favorite_count++], 160, "%s", e->id);
                    char all[22000] = "";
                    for (int i = 0; i < ha_favorite_count; i++) {
                        strcat(all, ha_favorites[i]);
                        strcat(all, "\n");
                    }
                    extra_write_text("ha-favorites.txt", all);
                } else if (e->sensor) {
                    snprintf(ha_plot_id, sizeof(ha_plot_id), "%s", e->id);
                    extra_open(60);
                    return 1;
                } else if (!strcmp(e->state, "unavailable") || !strcmp(e->state, "unknown"))
                    ui_message("Gerät nicht verfügbar.");
                else if (!service_start(SERVICE_TOGGLE, ha_url, ha_token, e->id))
                    ui_message("Anfrage läuft bereits.");
            }
        } else if (y >= 745 && y < 789) {
            if (x < 300 && ha_page)
                ha_page--;
            else if (x >= 300) {
                int view[512], n = ha_visible(view);
                if ((ha_page + 1) * (ha_select ? 6 : 8) < n)
                    ha_page++;
            }
        }
        return 1;
    }
    if (ui_route() == 34) {
        if (y >= 188 && y < 240)
            open_keyboard(31, "Home Assistant: URL", ha_url);
        else if (y >= 327 && y < 379)
            open_keyboard(32, "Zugriffstoken", ha_token);
        else if (y >= 607 && y < 655) {
            if (service_start(SERVICE_HA, ha_url, ha_token, "")) {
                ha_select = 1;
                ha_page = 0;
                extra_return(33);
            } else
                ui_message("Adresse und Token prüfen.");
        }
        return 1;
    }
    if (ui_route() == 51) {
        return 1;
    }
    if (ui_route() == 36)
        return wiki_history_input(in);
    if (ui_route() == 39) {
        if (y >= 286 && y < 348) {
            options[OPT_AUTO_TIME] ^= 1;
            library_settings_save();
            if (options[OPT_AUTO_TIME] && !service_busy())
                service_start(SERVICE_TIME, "", "", "");

        } else if (!options[OPT_AUTO_TIME] && y >= 387 && y < 441) {
            extra_open(47);
        } else if (!options[OPT_AUTO_TIME] && y >= 480 && y < 532)
            open_dropdown(100 + OPT_ZONE, zone_names, 5, options[OPT_ZONE], 220, 540, 350);
        return 1;
    }
    if (ui_route() == 47) {
        if (y >= 170 && y < 568) {
            int row = (y - 170) / 86, delta = x >= 480 ? 1 : x >= 390 ? -1 : 0;
            if (row == 0) {
                time_day += delta;
                int max = calendar_days(time_year, time_month);
                if (time_day < 1)
                    time_day = max;
                if (time_day > max)
                    time_day = 1;
            }
            if (row == 1) {
                time_month += delta;
                if (time_month < 1)
                    time_month = 12;
                if (time_month > 12)
                    time_month = 1;
            }
            if (row == 2) {
                time_year += delta;
                if (time_year < 2000)
                    time_year = 2000;
                if (time_year > 2100)
                    time_year = 2100;
            }
            if (row == 3)
                time_hour = (time_hour + delta + 24) % 24;
            if (row == 4)
                time_minute = (time_minute + delta + 60) % 60;
            int max = calendar_days(time_year, time_month);
            if (time_day > max)
                time_day = max;
        } else if (y >= 650 && y < 706) {
            if (system_set_time(time_year, time_month, time_day, time_hour, time_minute)) {
                status_ui.status_time = 0;
                extra_return(39);
            } else
                ui_message("Ungültiges Datum / Uhrzeit.");
        }
        return 1;
    }
    if (ui_route() == 41) {
        if (y >= 455 && y < 503) {
            library_defaults();
            extra_zone();
            apply_reading_settings();
            char p[2048];
            local_path(p, sizeof(p), "../DISABLE_AUTOSTART");
            unlink(p);
            route_to(13);
            history_count = 0;
            ui_message("Standardeinstellungen wiederhergestellt.");
        } else if (y >= 530 && y < 578)
            extra_return(40);
        return 1;
    }

    if(ui_route()==43){
        if(y>=455 && y<503){note_delete16(note_selected);notes_scan();extra_return(29);}
        else if(y>=530 && y<578)extra_return(29);
        return 1;
    }

    return 1;
}
