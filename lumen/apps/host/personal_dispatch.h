/* App-owned module; assembled by apps/host/compose.h. */
static void v1_open(int target) {
    polish_open(target);
    v1_init();
    v1_page = 0;
    if (target == 71)
        v1_photos_scan();
    if (target == 76)
        flash_next();
    if (target == 79) {
        static int ready;
        if (!ready) {
            sky_init();
            ready = 1;
        }
    }
    if (target == 85)
        markdown_page = 0;
    if (target == 87 && !timer_due && !timer_paused)
        timer_total_seconds = timer_minutes * 60;
    if (target == 90 || target == 91) { /* registered shell components open their configuration */
        setting_category = 1;
        settings_ui.setting_page = target == 90 ? 0 : 1;
    }
    if (target == 93)
        open_keyboard(130, "Tastatur", "");
}
static void v1_draw(void) {
    if (ui_route() >= 94) {
        polish_view();
        return;
    }
    switch (ui_route()) {
    case 71:
        v1_photos_draw();
        break;
    case 72:
        v1_photo_draw();
        break;
    case 73:
        v1_language_draw();
        break;
    case 74:
        v1_tasks_draw();
        break;
    case 76:
        v1_flash_draw();
        break;
    case 77:
        heading_actions("Kartenbibliothek", 1);
        icon_button(528, 74, IC_PLUS);
        for (int r = 0; r < 7 && flash_page * 7 + r < flash_count; r++) {
            int i = flash_page * 7 + r, y = 151 + r * 72;
            region(28, y, 474, 66);
            drawtext_raw(30, y, flashcards[i].front, ui_font(UI_BODY), 466);
            drawtext_raw(30, y + 33, flashcards[i].deck, ui_font(UI_SECONDARY), 466);
            icon_button(522, y + 8, IC_TRASH);
            divider(28, y + 66, 544);
        }
        pager(695, &flash_page, (flash_count + 6) / 7);
        break;
    case 78:
        heading("Karte bearbeiten");
        button_h(28, 180, 544, 60, "Sammlung", 0);
        button_h(28, 269, 544, 60, "Frage", 0);
        button_h(28, 358, 544, 60, "Antwort", 0);
        break;
    case 79:
        sky_draw();
        break;
    case 80:
        sky_settings_draw();
        break;
    case 83:
        v1_launcher_settings();
        break;
    case 84:
        v1_folder_picker_draw();
        break;
    case 85:
        reader_markdown17("Notizen",note_text,30);
        reader();
        break;
    case 86:
        v1_archive_draw();
        break;
    case 87:
        v1_timer_ring(1);
        break;
    case 90:
        more_settings();
        break;
    case 92:
        heading("Sperrbild");
        for (int i = 0; i < 5; i++)
            button_h(28, 154 + i * 86, 544, 65, lockmodes[i], options[OPT_LOCK] == lock_mode_ids[i]);
        break;
    case 93:
        heading("Tastatur");
        button_h(28, 180, 544, 60, "Text eingeben", 0);
        break;
    }
    if (v1_delete) {
        rect(21, 267, 564, 236, 190);
        rect(17, 263, 564, 236, 255);
        key_outline(17, 263, 564, 236);
        centered(28, 285, 544, 55, "Wirklich löschen?", ui_font(UI_TITLE));
        button_h(36, 400, 250, 58, "Abbrechen", 0);
        button_h(310, 400, 250, 58, "Löschen", 0);
    }
}
static int v1_commit(int target, const char *text) {
    if (target >= 150)
        return polish_commit(target, text);
    if (target == 110) {
        if (*text && task_count < 256) {
            snprintf(tasks[task_count].title, 200, "%s", text);
            task_recent_done[task_count]=0;
            tasks[task_count].created = time(NULL);
            tasks[task_count++].state = 0;
            task_page = task_tab = 0;
            tasks_save();
        }
        return 1;
    }
    if (target == 111) {
        if (!*text)
            return 1;
        snprintf(flash_deck, 64, "%s", text);
        open_keyboard(112, "Frage", flash_edit >= 0 ? flashcards[flash_edit].front : "");
        return 2;
    }
    if (target == 112) {
        if (!*text)
            return 1;
        snprintf(flash_front, 512, "%s", text);
        open_keyboard(113, "Antwort", flash_edit >= 0 ? flashcards[flash_edit].back : "");
        return 2;
    }
    if (target == 113) {
        if (!*text || (!*flash_front && flash_edit < 0))
            return 1;
        int i = flash_edit >= 0 ? flash_edit : flash_count;
        if (i >= 512) {
            ui_message("Kartenbibliothek voll.");
            return 1;
        }
        if (flash_edit < 0)
            memset(&flashcards[i], 0, sizeof(Flashcard));
        if (!flashcards[i].id)
            flashcards[i].id = (uint64_t)time(NULL) * 1000 + i;
        snprintf(flashcards[i].deck, 64, "%s", flash_deck);
        snprintf(flashcards[i].front, 512, "%s", flash_front);
        snprintf(flashcards[i].back, 1024, "%s", text);
        if (i == flash_count)
            flash_count++;
        v1_dirty = 1;
        flash_edit = -1;
        flash_next();
        return 1;
    }
    if (target == 120) {
        if (*text && note_tree.nf < 512) {
            int id = 1;
            for (int i = 0; i < note_tree.nf; i++)
                if (note_tree.folder[i].id >= id)
                    id = note_tree.folder[i].id + 1;
            NoteFolder *f = &note_tree.folder[note_tree.nf++];
            f->id = id;
            f->parent = note_tree.current;
            snprintf(f->name, 96, "%s", text);
            v1_dirty = 1;
        }
        return 1;
    }
    if (target == 121) {
        double lat, lon;
        char extra;
        if (sscanf(text, "%lf , %lf %c", &lat, &lon, &extra) == 2 && isfinite(lat) &&
            isfinite(lon) && fabs(lat) <= 90 && fabs(lon) <= 180) {
            sky_config.lat = lat;
            sky_config.lon = lon;
            double p[2] = {lat, lon};
            app_state_save("sky-location-1.bin", p, sizeof(p));
            sky_key[0] = 0;
        } else
            ui_message("Breitengrad, Längengrad prüfen.");
        return 1;
    }
    if (target == 122) {
        int Y, M, D, h, m;
        char extra;
        if (sscanf(text, "%d-%d-%d %d:%d %c", &Y, &M, &D, &h, &m, &extra) == 5 && Y >= 1 &&
            Y <= 12026 && M >= 1 && M <= 12 && D >= 1 && D <= calendar_days(Y, M) && h >= 0 &&
            h < 24 && m >= 0 && m < 60) {
            sky_config.year = Y;
            sky_config.month = M;
            sky_config.day = D;
            sky_config.hour = h;
            sky_config.minute = m;
            sky_key[0] = 0;
        } else
            ui_message("Datum: YYYY-MM-DD HH:MM UTC");
        return 1;
    }
    if (target == 130)
        return 1;
    return 0;
}
static int v1_tick(void) {
    if (!sleeping && ui_route() == 72 && photo_slideshow && photo_count &&
        extra_millis() >= photo_next_frame) {
        photo_selected = (photo_selected + 1) % photo_count;
        photo_next_frame = extra_millis() + photo_catalog.seconds * 1000;
        return 1;
    }
    if (polish_tick())
        return 1;
    if (alarm_ring) {
        if (!timer_flash_started) {
            timer_flash_started = extra_millis();
            timer_last_phase = -1;
        }
        int64_t phase = (extra_millis() - timer_flash_started) / 500;
        if (phase < 120 && phase != timer_last_phase) {
            timer_last_phase = phase;
            return 1;
        }
    } else
        timer_flash_started = 0;
    static int lastsecond;
    int sec = timer_due ? (timer_due - extra_millis()) / 1000 : 0;
    if ((ui_route() == 87 || ui_route() == 28) && sec != lastsecond) {
        lastsecond = sec;
        return 1;
    }
    return 0;
}
static int v1_poll(void) {
    if (ui_route() == 72 && photo_slideshow) {
        int64_t remaining = photo_next_frame - extra_millis();
        return remaining > 0 ? (int)remaining : 1;
    }
    if (ui_route() == 48 && chess_computer && chess.turn < 0)
        return 250;
    if (alarm_ring && timer_flash_started && extra_millis() - timer_flash_started < 60000)
        return 500;
    if (ui_route() == 87)
        return 1000;
    return -1;
}
static void v1_effect(void) {
    if (alarm_ring && timer_flash_started && extra_millis() - timer_flash_started < 60000 &&
        (timer_last_phase & 1))
        for (size_t i = 0; i < sizeof(ui_pixels); i++)
            ui_pixels[i] = 255 - ui_pixels[i];
}
static int v1_input(const Input *in) {
    if (ui_route() >= 94)
        return polish_input(in);
    if (ui_route() == 86 || ui_route() == 67)
        return polish_news_input(in);
    if (ui_route() == 72 && !v1_delete)
        return polish_photo_input(in);
    if (ui_route() == 73)
        return polish_language_input(in);
    if (in->kind != INPUT_TAP)
        return 2;
    int x = in->x, y = in->y;
    if (v1_delete) {
        if (y >= 400 && y < 458) {
            if (x >= 300) {
                if (v1_delete == 1 && photo_selected >= 0 && photo_selected < photo_count) {
                    char p[2048], n[300];
                    snprintf(n, sizeof(n), "photos/%s", photo_names[photo_selected]);
                    local_path(p, sizeof(p), n);
                    if (unlink(p)) {
                        ui_message("Löschen fehlgeschlagen.");
                        v1_delete = 0;
                        return 1;
                    }
                    PhotoMembership *member = photo_membership(photo_id(photo_names[photo_selected]), 0);
                    if (member) {
                        *member = (PhotoMembership){0};
                        photo_catalog_save();
                    }
                    char cache[2048], thumb[300];
                    snprintf(thumb, sizeof(thumb), "photos/.thumbs/%s",
                             photo_names[photo_selected]);
                    local_path(cache, sizeof(cache), thumb);
                    unlink(cache);
                    v1_photos_scan();
                    extra_return(71);
                }
                if (v1_delete == 2 && flash_edit >= 0 && flash_edit < flash_count) {
                    memmove(&flashcards[flash_edit], &flashcards[flash_edit + 1],
                            (flash_count - flash_edit - 1) * sizeof(Flashcard));
                    memset(&flashcards[--flash_count], 0, sizeof(Flashcard));
                    v1_dirty = 1;
                }
                if (v1_delete >= 1000) {
                    int i = v1_delete - 1000;
                    if (i < task_count) {
                        memmove(&tasks[i], &tasks[i + 1], (task_count - i - 1) * sizeof(tasks[0]));
                        memmove(task_recent_done+i,task_recent_done+i+1,(size_t)(task_count-i-1));
                        memset(&tasks[--task_count], 0, sizeof(tasks[0]));
                        task_recent_done[task_count]=0;
                        tasks_save();
                    }
                }
            }
            v1_delete = 0;
        }
        return 1;
    }
    switch (ui_route()) {
    case 71:
        photo_grid_input(x, y);
        return 1;
    case 73:
        if (y >= 135 && y < 179) {
            lang_pair = (x - 28) / (544 / 3);
            if (lang_pair < 0)
                lang_pair = 0;
            if (lang_pair > 2)
                lang_pair = 2;
            lang_reveal = 0;
        } else if (!lang_reveal && y >= 537 && y < 597)
            lang_reveal = 1;
        else if (lang_reveal && y >= 610 && y < 666) {
            lang_total++;
            v1_dirty = 1;
            if (x >= 300)
                lang_correct++;
            lang_index = (lang_index + 1) % 24;
            lang_reveal = 0;
        }
        break;
    case 74:
        return polish_tasks_input(in);
    case 76:
        if (y >= 74 && y < 118) {
            if (x >= 516) {
                flash_edit = -1;
                open_keyboard(111, "Sammlung", flash_deck);
            } else if (x >= 456)
                extra_open(77);
        } else if (flash_selected >= 0) {
            if (!flash_reveal && y >= 622 && y < 682)
                flash_reveal = 1;
            else if (flash_reveal && y >= 722 && y < 776 && x >= 28 && x < 572) {
                int grade = (x - 28) / 140 + 1;
                if (grade <= 4) {
                    fsrs_review(&flashcards[flash_selected].memory, grade, time(NULL));
                    char path[2048];
                    local_path(path, sizeof(path), "flashcard-reviews.tsv");
                    FILE *log = fopen(path, "a");
                    if (log) {
                        MemoryCard *m = &flashcards[flash_selected].memory;
                        fprintf(log, "%lld\t%llu\t%d\t%.8g\t%.8g\t%lld\n", (long long)m->last,
                                (unsigned long long)flashcards[flash_selected].id, grade,
                                m->stability, m->difficulty, (long long)m->due);
                        fclose(log);
                    }
                    v1_dirty = 1;
                    flash_next();
                }
            }
        }
        break;
    case 77:
        if (y >= 74 && y < 118 && x >= 516) {
            flash_edit = -1;
            open_keyboard(111, "Sammlung", flash_deck);
        } else if (y >= 151 && y < 655) {
            int i = flash_page * 7 + (y - 151) / 72;
            if (i < flash_count) {
                flash_edit = i;
                if (x >= 510)
                    v1_delete = 2;
                else {
                    snprintf(flash_deck, 64, "%s", flashcards[i].deck);
                    snprintf(flash_front, 512, "%s", flashcards[i].front);
                    open_keyboard(111, "Sammlung", flash_deck);
                }
            }
        } else if (y >= 695 && y < 739) {
            if (x < 300 && flash_page)
                flash_page--;
            else if (x >= 300 && (flash_page + 1) * 7 < flash_count)
                flash_page++;
        }
        break;
    case 79:
        if (y >= 74 && y < 118 && x >= 516) {
            extra_open(80);
            break;
        }
        if (y >= 696 && y < 734) {
            if (x < 80)
                sky_shift(-1);
            else if (x >= 520)
                sky_shift(1);
            else {
                sky_step = (x - 83) / (434 / 4);
                if (sky_step > 3)
                    sky_step = 3;
                sky_shift(1);
            }
            if (sky_step < 0)
                sky_step = 0;
            if (sky_step > 3)
                sky_step = 3;
        } else if (y >= 747) {
            if (x < 200)
                sky_now();
            else if (x < 390) {
                sky_panel = 1;
                extra_open(80);
            } else {
                char b[80];
                snprintf(b, 80, "%04d-%02d-%02d %02d:%02d", sky_config.year, sky_config.month,
                         sky_config.day, sky_config.hour, sky_config.minute);
                open_keyboard(122, "Datum · UTC", b);
            }
        } else if (y >= 158 && y < 693) {
            int best = 625;
            sky_selected = -1;
            for (unsigned i = 0; i < sizeof(sky_stars) / sizeof(*sky_stars); i++) {
                int dx = x - sky_px[i], dy = y - sky_py[i], d = dx * dx + dy * dy;
                if (d < best) {
                    best = d;
                    sky_selected = i;
                }
            }
        }
        break;
    case 80:
        if (y >= 136 && y < 180) {
            sky_panel = (x - 28) / (544 / 3);
            if (sky_panel < 0)
                sky_panel = 0;
            if (sky_panel > 2)
                sky_panel = 2;
        } else if (!sky_panel) {
            if (y >= 228 && y < 284) {
                char b[100];
                snprintf(b, 100, "%.4f, %.4f", sky_config.lat, sky_config.lon);
                open_keyboard(121, "Breitengrad, Längengrad", b);
            } else if (y >= 325 && y < 375 && x >= 460) {
                sky_show_lines ^= 1;
                sky_key[0] = 0;
            }
        } else if (sky_panel == 2 && y >= 708 && y < 752) {
            if (x < 300 && v1_page)
                v1_page--;
            else if (x >= 300 && v1_page < 4)
                v1_page++;
        }
        break;
    case 83: {
        int indices[APP_LIMIT], count = launcher_manage_indices(indices);
        if (y >= 174 && y < 650) {
            int pos = launcher_settings_page * 8 + (y - 174) / 68;
            if (pos < count) {
                int i = indices[pos], id = launcher_order[i];
                if(x>=526 && x<570 && app_package_can_remove(id)) {
                    static const char *actions[]={"Abbrechen","App endgültig entfernen"};
                    manage_delete_id=id;
                    open_dropdown(2004,actions,2,-1,315,y+20>675 ? 675 : y+20,258);
                } else if(x>=427 && x<503)
                    launcher_visible[id]^=1;
                else if (x >= 329 && x < 373 && pos > 0) {
                    int other = indices[pos - 1];
                    launcher_order[i] = launcher_order[other];
                    launcher_order[other] = id;
                } else if (x >= 375 && x < 419 && pos + 1 < count) {
                    int other = indices[pos + 1];
                    launcher_order[i] = launcher_order[other];
                    launcher_order[other] = id;
                }
                v1_dirty = 1;
            }
        } else if (y >= 745 && y < 789) {
            if (x < 300 && launcher_settings_page)
                launcher_settings_page--;
            else if (x >= 300 && (launcher_settings_page + 1) * 8 < count)
                launcher_settings_page++;
        }
        break;
    }
    case 84:
        if (y >= 145 && y < 641) {
            int i = folder_page * 8 + (y - 145) / 62;
            if (i <= note_tree.nf) {
                v1_place(note_selected, i ? note_tree.folder[i - 1].id : 0);
                extra_return(30);
            }
        } else if (y >= 699 && y < 743) {
            if (x < 300 && folder_page)
                folder_page--;
            else if (x >= 300 && (folder_page + 1) * 8 < note_tree.nf + 1)
                folder_page++;
        }
        break;
    case 85:
        if (y >= 57 && y < 101 && x >= 516)
            extra_return(30);
        else if (y >= 746) {
            if (x < 300 && markdown_page)
                markdown_page--;
            else if (x >= 300 && markdown_page + 1 < markdown_pages)
                markdown_page++;
        }
        break;
    case 86:
        if (y >= 153 && y < 197 && x >= 516)
            extra_open(68);
        else if (y >= 223 && y < 679) {
            int i = news_archive_page * 6 + (y - 223) / 76;
            if (i < news_date_count) {
                int date = news_dates[i];
                if (news_load(date)) {
                    extra_open(67);
                } else {
                    news_start(options[OPT_LANGUAGE], news_topics);
                    extra_open(67);
                }
            }
        } else if (y >= 710 && y < 754) {
            if (x < 300 && news_archive_page)
                news_archive_page--;
            else if (x >= 300 && (news_archive_page + 1) * 6 < news_date_count)
                news_archive_page++;
        }
        break;
    case 87:
        if (y >= 665 && y < 725) {
            timer_action(x >= 300);
            extra_save();
        }
        break;
    case 90:
        settings_input(in);
        break;
    case 92:
        if (y >= 154 && y < 584) {
            int i = (y - 154) / 86;
            if (i < 5) {
                options[OPT_LOCK] = lock_mode_ids[i];
                ui_save();
            }
        }
        break;
    case 93:
        if (y >= 180 && y < 240)
            open_keyboard(130, "Tastatur", "");
        break;
    }
    return 1;
}
static pid_t chess_worker = -1;
static ChessPosition chess_job;
static unsigned chess_nodes;
