/* App view and event dispatch. Status and tab bars own their views in their app modules. */
static void app_render(void) {
    if (ui_route() == 132) { more_settings(); return; }
    if (app_page_draw(ui_route())) return;
    if (ui_route() >= 60) {
        new_apps_draw();
        return;
    }
    switch (ui_route()) {
    case 0:
        library_screen();
        break;
    case 1:
        library_screen();
        break;
    case 2:
        reader();
        break;
    case 3:
        settings_screen();
        break;
    case 4:
        chapters_screen();
        break;
    case 5:
        menu();
        break;
    case 6:
        search_screen();
        break;
    case 7:
        detail();
        break;
    case 9:
        more_settings();
        break;
    case 10:
        discover_screen();
        break;
    case 11:
        discover_detail();
        break;
    case 12:
        wifi_screen();
        break;
    case 13:
        settings_home();
        break;
    case 14:
        annotations_screen();
        break;
    case 15:
        wikipedia_screen();
        break;
    default:
        if (ui_route() >= 27)
            extra_draw();
        break;
    }
}
static int app_handle_input(const Input *in) {
    if(ui_route()==131 && !app_package_installed(APP_HOME) && in->kind==INPUT_TAP &&
       in->x>=28 && in->x<572) {
        if(in->y>=280 && in->y<328) {
            AppIntent intent={.app=APP_SETTINGS,.kind=INTENT_OPEN};
            app_send(&intent);
        } else if(in->y>=345 && in->y<393) {
            AppIntent intent={.app=APP_FILE_TRANSFER,.kind=INTENT_OPEN};
            app_send(&intent);
        }
        return 1;
    }
    if (ui_route() == 132) { settings_input(in); return 1; }
    int x = in->x, y = in->y, old = ui_route();
    int handled = app_page_input(ui_route(), in);
    if (handled >= 0) return handled;
    if (ui_route() >= 60) return new_apps_input(in);

    if (ui_route() == 40)
        return system_input(in);
    if (ui_route() >= 27 || ui_route() == 22)
        return extra_input(in);
    if (ui_route() == 13 && in->kind == INPUT_TAP && y >= 134 && y < 494) {
        int row = (y - 134) / 72;
        settings_ui.setting_page = 0;
        if (history_count < 64)
            history[history_count++] = 13;
        if (row == 0) {
            apps_pause();
            route_to(12);
            wifi_open();
        } else if (row == 1) {
            setting_category = 1;
            route_to(9);
        } else if (row == 2)
            extra_open(39);
        else if (row == 3)
            extra_open(83);
        else
            extra_open(40);
        return 1;
    }
    if (ui_route() == 1 && in->kind == INPUT_TAP) {
        if (x >= 528 && x < 572 && y >= 60 && y < 104) {
            setting_category = 2;
            extra_open(132);
            return 1;
        }
        if (y >= 745 && y < 789) {
            if (x >= 190 && x < 259 && library_ui.listpage)
                library_ui.listpage--;
            else if (x >= 337 && x < 406 && library_ui.listpage + 1 < library_pages())
                library_ui.listpage++;
        } else if (featured() >= 0 && library_ui.listpage == 0 && y >= 80 && y < 312)
            app_read_book(featured());
        else if (y >= library_top() && y < 733 && x >= 28 && x < 572) {
            int slot = ((y - library_top()) / library_row_height()) * 3 + (x - 28) / 188,
                idx = library_begin() + slot;
            if (slot >= 0 && slot < library_per() && idx < library_ui.view_count)
                app_read_book(library_ui.view[idx]);
        }
        return 1;
    }
    if (ui_route() == 12) {
        wifi_input(in);
        return wifi_ui.wifi_exit ? 0 : 1;
    }
    if (ui_route() == 2 && in->kind == INPUT_TAP) {
        if (reader_ui.reader_chrome && y >= 74 && y < 118) {
            if ((x >= READER_CONTENTS_X && x < READER_CONTENTS_X + 44) ||
                (x >= READER_SETTINGS_X && x < READER_SETTINGS_X + 44)) {
                if (history_count < 64) history[history_count++] = 2;
                if (x < READER_SETTINGS_X) {
                    route_to(4);
                    reader_ui.chapterpage = 0;
                } else {
                    reader_ui.reading_page = 0;
                    route_to(3);
                }
            }
        } else if (y < 78 || y >= 722)
            reader_ui.reader_chrome = !reader_ui.reader_chrome;
        else {
            if (reader_ui.reader_chrome) {
                reader_ui.reader_chrome = 0;
                return 1;
            }
            turn(x < (options[OPT_TAP] ? 80 : 200) ? -1 : 1);
        }
        return 1;
    }
    if (ui_route() == 3) {
        reading_input(in);
        return 1;
    }
    if (ui_route() == 9) {
        settings_input(in);
        return wifi_ui.wifi_exit ? 0 : 1;
    }
    if (ui_route() == 10 || ui_route() == 11) {
        discover_input(in);
        return 1;
    }
    if (in->kind == INPUT_SWIPE) {
        if (ui_route() == 2) {
            if (!options[OPT_SWIPE])
                return 2;
            if (zoom > 1) {
                panx += in->dx;
                pany += in->dy;
                float limit = 600 * (zoom - 1);
                if (panx > limit)
                    panx = limit;
                if (panx < -limit)
                    panx = -limit;
                if (pany > limit)
                    pany = limit;
                if (pany < -limit)
                    pany = -limit;
            }
        }
        return 1;
    }
    if (ui_route() == 4) {
        int count = reader_ui.chaptertab ? library[selected].bookmark_count : reader_ui.chapter_count;
        if (y >= 145 && y < 193) {
            reader_ui.chaptertab = x > 300;
            reader_ui.chapterpage = 0;
        } else if (y >= 218 && y < 674) {
            int idx = reader_ui.chapterpage * 8 + (y - 218) / 57;
            if (idx < count) {
                page = reader_ui.chaptertab ? library[selected].bookmarks[idx] : reader_ui.chapters[idx].page;
                if (page >= engine_count())
                    page = engine_count() - 1;
                route_to(2);
                library_progress(&library[selected], page, engine_count());
            }
        } else if (y >= 678 && y < 726) {
            if (x < 180)
                route_to(2);
            else if (x < 390) {
                if (reader_ui.chapterpage)
                    reader_ui.chapterpage--;
            } else if ((reader_ui.chapterpage + 1) * 8 < count)
                reader_ui.chapterpage++;
        }
    } else if (ui_route() == 5) {
        if (y >= 270 && y < 318) {
            library[selected].favorite = !library[selected].favorite;
            library_save_book(&library[selected]);
        } else if (y >= 335 && y < 383)
            toggle_bookmark();
        else if (y >= 400 && y < 448)
            route_to(7);
        else if (y >= 465 && y < 513)
            route_to(1);
        else if (y >= 530 && y < 578) {
            route_to(14);
            reader_ui.annotation_page = 0;
        } else if (y >= 595 && y < 643)
            return 0;
        else if (y >= 665 && y < 713)
            route_to(2);
    } else if (ui_route() == 6) {
        if (y >= 320 && y < 515) {
            int col = (x - 28) / 55, row = (y - 320) / 65;
            if (x >= 28 && col >= 0 && col < 10 && row >= 0 && row < 3) {
                const char *key = keys[row * 10 + col];
                if (strlen(library_ui.query) + strlen(key) < sizeof(library_ui.query))
                    strcat(library_ui.query, key);
            }
        } else if (y >= 530 && y < 578) {
            size_t n = strlen(library_ui.query);
            if (x < 240) {
                if (n + 1 < sizeof(library_ui.query))
                    strcat(library_ui.query, " ");
            } else if (x < 410 && n) {
                do {
                    n--;
                } while (n && ((unsigned char)library_ui.query[n] & 192) == 128);
                library_ui.query[n] = 0;
            } else if (x >= 410)
                library_ui.query[0] = 0;
        } else if (y >= 650) {
            library_ui.listpage = 0;
            update_view();
            route_to(1);
        }
    } else if (ui_route() == 7) {
        if (y >= 385 && y < 433) {
            if (library_supported(&library[selected]))
                app_read_book(selected);
            else {
                ui_save();
                amazon_reader_request(library[selected].path);
                return 0;
            }
        } else if (y >= 451 && y < 499) {
            if (x < 300) {
                library[selected].favorite = !library[selected].favorite;
                library_save_book(&library[selected]);
            } else {
                const char *name = strrchr(library[selected].path, '/');
                open_keyboard(1, "Datei umbenennen", name ? name + 1 : library[selected].title);
            }
        } else if (y >= 517 && y < 565) {
            if (x < 300)
                open_keyboard(2, "Buchtitel bearbeiten", library[selected].title);
            else
                open_keyboard(3, "Autor bearbeiten", library[selected].author);
        } else if (y >= 583 && y < 631) {
            if (x < 300) {
                confirm_delete = 1;

            } else {
                route_to(1);
                update_view();
            }
        }
    }

    if (ui_route() != old) {
        if (ui_route() == 1)
            history_count = 0;
        else {
            int existing = -1;
            for (int i = 0; i < history_count; i++)
                if (history[i] == ui_route())
                    existing = i;
            if (existing >= 0)
                history_count = existing;
            else if (history_count < 64)
                history[history_count++] = old;
        }
    }
    return 1;
}
