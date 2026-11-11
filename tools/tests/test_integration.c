#define _GNU_SOURCE
#include "../../lumen/system/ui/ui.c"
#include <assert.h>
static void settle_app(void) {
    if(!app_package_installed(app_for_route(ui_route())))return;
    app_process_open(app_for_route(ui_route()));
    for(int i=0;i<30;i++) {
        if(!app_package_installed(app_for_route(ui_route())))break;
        usleep(5000);
        package_ui_poll();
    }
}
static void tap17(int x, int y) {
    Input in = {.kind = INPUT_TAP, .x = x, .y = y};
    ui_input(&in);
    settle_app();
    ui_draw();
}
static void shot17(const char *n) {
    notification_dismiss();
    ui_draw();
    char p[256];
    snprintf(p, 256, "build/tests/%s.pgm", n);
    FILE *f = fopen(p, "wb");
    fprintf(f, "P5\n600 800\n255\n");
    fwrite(ui_pixels, 1, 480000, f);
    fclose(f);
}
#include "test_input_ui.h"
#include "test_improvements.h"
#include "test_quality.h"
int main(int argc, char **argv) {
    (void)argv;
    library_root = "build/tests/fixtures";
    data_root = "build/tests/test-data";
    mkdir(data_root, 0700);
    assert(engine_init());
    assert(atomic_write("build/tests/test-data/photo-default-seeded","1",1));
    ui_init();
    for(int i=0;i<200 && !keyboard_layout_serial;i++){usleep(3000);keyboard_package_poll();}
    assert(keyboard_layout_serial);
    assert(!app_route_valid(124));
    if (argc > 1) {
        selected_recipe17.meal = 0;
        strcpy(selected_recipe17.lang, "de");
        strcpy(selected_recipe17.id, "wiki-de-115892");
        strcpy(selected_recipe17.title, "Rote-Bete-Gnocchi mit Spinat");
        assert(recipes_fetch17(""));
        recipe_archive16();
        assert(recipe_recent_count16 > 0);
        char path[4096];
        snprintf(path, sizeof(path), "%s/recipe-archive16/wiki-de-115892.html", data_root);
        assert(file_open16(path, 114));
        assert(engine_count() > 0);
        ui_draw();
        shot17("recipe-live");
        ui_close();
        engine_shutdown();
        puts("PASS native German recipe fetch, rich HTML, image preparation, offline archive and "
             "Reader");
        return 0;
    }
    route_to(29);
    ui_draw();
    u16_notes_defaults();
    note_tree.current = note_tree.folder[0].id;
    ui_draw();
    assert(u17_notes_depth());
    tap17(20, 20);
    assert(!note_tree.current);
    note_selected = -1;
    extra_commit(30, "Title\nSome text to edit.");
    int id = note_selected;
    route_to(29);
    ui_draw();
    shot17("notes");
    u17_notes_menu(0, id, 150);
    ui_draw();
    tap17(drop_x + 100, drop_y + 25);
    assert(dropdown && drop_target == 936);
    ui_draw();
    shot17("folder-popup");
    tap17(drop_x + 100, drop_y + 5 + 48 + 20);
    assert(v1_folder_for(id) == note_tree.folder[0].id);
    task_count = 1;
    strcpy(tasks[0].title, "Task");
    route_to(74);
    ui_draw();
    tap17(160, 165);
    assert(editor_ui.keyboard && editor_ui.keyboard_target == 180 && tasks[0].state == 0);
    strcpy(editor_ui.edit, "Edited task");
    commit_keyboard();
    ui_draw();
    tap17(42, 171);
    assert(tasks[0].state == 2);
    shot17("todo");
    task_count = 3;
    memset(task_recent_done,0,sizeof(task_recent_done));
    for (int i = 0; i < task_count; i++) {
        strcpy(tasks[i].title, "Task");
        tasks[i].state = 0;
    }
    ui_draw();
    assert(task_rows18[0].index == 2 && task_rows18[1].index == 1);
    tap17(479, task_rows18[2].y + task_rows18[2].h/2);
    assert(tasks[0].state & TASK_FLAG);
    assert(task_rows18[0].index == 0 && task_rows18[1].index == 2);
    tap17(42, task_rows18[0].y + task_rows18[0].h/2);
    assert(tasks[0].state == (TASK_FLAG | TASK_DONE));
    route_to(100);
    ui_draw();
    shot17("calc-settings");
    tap17(400, 225);
    assert(dropdown && drop_target == 931);
    tap17(drop_x + 80, drop_y + 5 + 48 + 15);
    assert(!calc_degrees);
    route_to(9);
    setting_category=1;settings_ui.setting_page=2;
    ui_draw();
    tap17(450,525);
    assert(dropdown && ui_route()==9 && drop_target==917);
    dropdown = 0;
    note_load(id);
    extra_page = 0;
    route_to(30);
    ui_draw();
    shot17("editor");
    Input hold = {.kind = INPUT_HOLD, .x = 300, .y = 600};
    route_to(27);
    ui_draw();
    ui_input(&hold);
    assert(!editor_ui.clip_active);
    open_keyboard(180, "Task", "some text");
    ui_draw();
    hold.x = 100;
    hold.y = 540;
    ui_input(&hold);
    assert(!editor_ui.clip_active);
    editor_ui.keyboard = 0;
    for (int level = 1; level <= 20; level++) {
        warehouse_new17(level);
        int goals = 0, boxes = 0;
        for (int i = 0; i < warehouse17.w * warehouse17.h; i++) {
            goals += warehouse17.board[i] == 2 || warehouse17.board[i] == 5;
            boxes += box17(warehouse17.board[i]);
        }
        assert(goals == boxes && boxes > 0);
        assert(floor17(warehouse17.board[warehouse17.y * warehouse17.w + warehouse17.x]));
    }
    route_to(118);
    ui_draw();
    shot17("sokoban");
    route_to(119);
    settle_app();
    assert(strstr(app_process_frame(),"Maze") || strstr(app_process_frame(),"Labyrinth"));
    ui_draw();
    shot17("maze");
    route_to(120);
    ui_draw();
    shot17("crossword");
    crossword_pick16 = 0;
    cross_commit17("X");
    assert(!*crossword_answer16[0]);
    cross_commit17(cross_words16[0]);
    assert(!strcmp(crossword_answer16[0], cross_words16[0]));
    route_to(110);
    ui_draw();
    shot17("journal-list");
    int today = app_date_key(time(NULL));
    journal_open17(today);
    ui_draw();
    u17_more_commit(171, "My day\nSecond line");
    journal_score16 = 4;
    journal_store17();
    extra_return(110);
    journal_open17(today);
    assert(!strcmp(journal_answers16[0], "My day\nSecond line") && journal_score16 == 4);
    ui_draw();
    shot17("journal");
    extra_write_text("llm-models17.json", "{\"data\":[{\"id\":\"model-b\"},{\"id\":\"model-a\"}]}");
    models_parse17();
    assert(model_count17 == 2 && !strcmp(model_ids17[0], "model-a"));
    models_loaded17 = 1;
    extra_write_text("llm-key.txt", "dummy-visible-key");
    route_to(123);
    ui_draw();
    shot17("llm-settings");
    route_to(122);
    ui_draw();
    shot17("llm");
    reader_markdown17("Markdown test", "# Heading\n\nA **bold** paragraph.\n\n- One\n- Two\n", 122);
    assert(ui_route() == 2 && reader_ui.reader_document_active);
    reader_ui.reader_chrome = 1;
    ui_draw();
    shot17("reader");
    int page_before = page;
    tap17(300, 300);
    assert(!reader_ui.reader_chrome && page == page_before);
    tap17(300, 55);
    assert(reader_ui.reader_chrome);
    tap17(100, 128);
    assert(ui_route() == 2);
    route_to(2);
    reader_ui.reader_chrome = 1;
    ui_draw();
    tap17(550, 95);
    assert(ui_route() == 3);
    tap17(314, 766);
    assert(reader_ui.reading_page == 1);
    tap17(480, 245);
    assert(dropdown && drop_target == 916);
    dropdown = 0;
    calendar_view16 = 1;
    route_to(21);
    ui_draw();
    shot17("calendar-week");
    tap17(200, 350);
    assert(calendar_view16 == 0);
    ui_draw();
    shot17("calendar-day");
    route_to(79);
    extra_prepare(79);
    ui_draw();
    tap17(400, 90);
    assert(dropdown && drop_target == 933);
    tap17(drop_x + 90, drop_y + 5 + 48 + 20);
    assert(ui_route() == 125);
    ui_draw();
    shot17("solar");
    for (int lang = 0; lang < 3; lang++) {
        options[OPT_LANGUAGE] = lang;
        route_to(123);
        ui_draw();
        shot17(lang == 0 ? "de" : lang == 1 ? "en" : "fr");
        assert(strcmp(tr_language("Dieser Ordner ist leer.", lang),
                      lang ? "Dieser Ordner ist leer." : "English") != 0);
    }
    options[OPT_LANGUAGE] = 0;
    input_tests18();
    improvements_tests();
    quality_tests();
    route_to(130);
    ui_draw();
    for(int i=0;i<100&&!*app_process_frame();i++){usleep(10000);package_ui_poll();}
    ui_draw();
    static unsigned char frog_preview[540 * WIDTH];
    memcpy(frog_preview, ui_pixels + 135 * WIDTH, sizeof(frog_preview));
    tap17(145, 405);
    tap17(299, 405);
    assert(memcmp(frog_preview, ui_pixels + 135 * WIDTH, sizeof(frog_preview)));
    tap17(490, 96);
    assert(!memcmp(frog_preview, ui_pixels + 135 * WIDTH, sizeof(frog_preview)));
    route_to(28);
    timer_panel = 0;
    ui_draw();
    tap17(550, 159);
    assert(clock_ui.clock_fullscreen);
    tap17(300, 400);
    assert(!clock_ui.clock_fullscreen);
    char translation[420];
    assert(dictionary_translate("bonjour", 2, translation, sizeof(translation)));
    assert(!strncmp(translation, "fra-eng", 7));

    /* Real gallery entry, folder editor, long press assignment and timed playback. */
    char photo_path[2048];
    local_path(photo_path, sizeof(photo_path), "photos");
    mkdir(photo_path, 0700);
    for (int i = 0; i < 2; i++) {
        char file[2100];
        snprintf(file, sizeof(file), "%s/demo-%d.pgm", photo_path, i);
        FILE *photo = fopen(file, "wb");
        assert(photo);
        fprintf(photo, "P5\n600 800\n255\n");
        static unsigned char raster[WIDTH*HEIGHT];
        memset(raster, 80 + i * 20, sizeof(raster));
        assert(fwrite(raster, 1, sizeof(raster), photo) == sizeof(raster));
        fclose(photo);
    }
    extra_open(71);
    notification_dismiss();
    ui_draw();
    assert(photo_count == 2);
    tap17(549, 96);
    assert(photo_settings && ui_route() == 71);
    tap17(20, 20);
    assert(!photo_settings && ui_route() == 71);
    tap17(430, 96);
    assert(editor_ui.keyboard && editor_ui.keyboard_target == TEXT_INPUT_PHOTO_FOLDER);
    strcpy(editor_ui.edit, "Test folder");
    commit_keyboard();
    ui_draw();
    assert(photo_folder == 1 && !photo_count);
    tap17(100, 156);
    tap17(drop_x + 85, drop_y + 25);
    assert(!photo_folder && photo_count == 2);
    Input photo_hold = {.kind=INPUT_HOLD, .x=80, .y=240};
    assert(ui_input(&photo_hold) == 1 && dropdown && drop_target == 940);
    ui_draw();
    tap17(80,240);
    assert(dropdown && drop_target==940);
    tap17(drop_x+85,drop_y+25);
    assert(dropdown && drop_target==904);
    ui_draw();
    tap17(drop_x + 85, drop_y + 25);
    assert(photo_count == 2 && !dropdown);
    tap17(100, 156);
    tap17(drop_x + 85, drop_y + 73);
    assert(photo_folder == 1 && photo_count == 1);
    tap17(450, 156);
    assert(ui_route() == 72 && photo_slideshow && polish_photo_full());
    photo_next_frame = extra_millis() - 1;
    assert(v1_tick() == 1);
    tap17(300, 300);
    assert(ui_route() == 71 && !photo_slideshow && photo_folder == 1);
    options[OPT_DATE] = options[OPT_CLOCK] = 1;
    ui_draw();
    for(int i=0;i<100 && !status_ui.status_date_w;i++){usleep(3000);status_overlay_poll();ui_draw();}
    assert(status_ui.status_date_w && status_ui.status_clock_w);
    tap17(status_ui.status_clock_x + 10, 20);
    assert(ui_route() == 28 && timer_panel == 0);
    tap17(20, 20);
    assert(ui_route() == 71);
    tap17(status_ui.status_date_x + 10, 20);
    assert(ui_route() == 21);
    tap17(20, 20);
    assert(ui_route() == 71);

    extra_open(33);
    ha_select = 1;
    int ha_depth = history_count;
    ui_draw();
    tap17(20, 20);
    assert(ui_route() == 33 && !ha_select && history_count == ha_depth);
    tap17(20, 20);
    assert(ui_route() == 71);

    options[OPT_BATTERY] = 1;
    ui_draw();
    tap17(status_ui.status_battery_x + 10, 20);
    assert(battery_popup);
    tap17(549, 96);
    assert(!battery_popup && !photo_settings && ui_route() == 71);

    memset(ui_pixels, 255, sizeof(ui_pixels));
    options[OPT_ROUNDING] = 1;
    widget_count = 0;
    button_h(20, 20, 100, 48, "", 0);
    unsigned char rim = ui_pixels[22*WIDTH+22];
    pressed = 0;
    ui_feedback_draw();
    assert(ui_pixels[22*WIDTH+22] == rim && ui_pixels[42*WIDTH+70] == 0);
    pressed = -1;
    memset(ui_pixels, 255, sizeof(ui_pixels));
    control_surface(20, 20, 48, 48, 16, 0);
    assert(ui_pixels[20*WIDTH+20] > 0);
    options[OPT_ROUNDING] = 0;
    control_surface(20, 20, 48, 48, 16, 0);
    assert(ui_pixels[20*WIDTH+20] == 0);
    options[OPT_ROUNDING] = 1;
    route_to(33);
    strcpy(ha_url, "http://example.invalid");
    strcpy(ha_token, "fixture");
    ha_select = 1;
    ha_count = 2;
    strcpy(ha_entities[0].name, "Küche");
    strcpy(ha_entities[0].id, "light.kitchen");
    strcpy(ha_entities[1].name, "Salon");
    strcpy(ha_entities[1].id, "sensor.living_room");
    ui_draw();
    tap17(150, 210);
    assert(editor_ui.keyboard && editor_ui.keyboard_target == 61);
    keyboard_insert("KUCHE");
    keyboard_layout18();
    for (int k = 0; k < keyboard_count18; k++)
        if (keyboard_keys18[k].action == KEY_ENTER18) {
            tap17(keyboard_keys18[k].x + 8, keyboard_keys18[k].y + 8);
            break;
        }
    int matches[512];
    assert(!editor_ui.keyboard && ui_route() == 33 && ha_visible(matches) == 1 && matches[0] == 0);
    extra_commit(61, "missing");
    ui_draw();
    assert(ha_visible(matches) == 0);

    route_to(122);
    chat_clear();
    const char *long_reply = "Line one.\nLine two.\nLine three.\nLine four.\nLine five.\nLine six.";
    for (int i=0; i<8; i++)
        assert(chat_append(i%2, i%2 ? long_reply : "Short message."));
    ui_draw();
    assert(chat_pages > 1 && !chat_rows[0].truncated && chat_rows[1].truncated);
    tap17(chat_pages <= 4 ? pager_dot_start(PAGER_X,PAGER_W,chat_pages)+45 : 370, CHAT_PAGER_Y + 22);
    assert(chat_page == 1);
    tap17(chat_pages <= 4 ? pager_dot_start(PAGER_X,PAGER_W,chat_pages)+15 : 220, CHAT_PAGER_Y + 22);
    assert(chat_page == 0);
    tap17(475, chat_rows[1].y + 15);
    assert(ui_route() == 2 && reader_ui.reader_document_active);
    tap17(300, 55);
    tap17(20, 20);
    assert(ui_route() == 122 && chat_page == 0);
    route_to(128);
    chat_archive_count = 14;
    ui_draw();
    tap17(314, 768);
    assert(chat_archive_page == 1);
    route_to(131);
    options[OPT_DATE] = options[OPT_CLOCK] = options[OPT_WIFI] = 1;
    assert(ui_prepare_home());
    ui_draw();
    for(int i=0;i<100 && !status_ui.status_date_w;i++){usleep(3000);status_overlay_poll();ui_draw();}
    assert(status_ui.status_date_x < status_ui.status_clock_x &&
           status_ui.status_clock_x < status_ui.status_wifi_x);
    assert(package_pager.count>1 && package_pager.page==0);
    int home_dots = pager_dot_start(PAGER_X,PAGER_W,package_pager.count);
    tap17(home_dots+45, 764);
    assert(package_pager.page==1);
    tap17(home_dots+15, 764);
    assert(package_pager.page==0);
    tap17(300,300);
    assert(ui_route() == 131 && package_pager.page==0);
    int home_ids[APP_LIMIT];
    int visible=v1_visible(home_ids);
    assert(visible>8);
    assert(!memcmp(home_ids, launcher_first, sizeof(launcher_first)));
    int notifications=app_package_find("lumen.notifications");
    assert(notifications>=0 && !launcher_visible[notifications]);
    assert(app_package_installed(APP_HOME) && app_package_installed(APP_FILE_TRANSFER));
    assert(options[OPT_APP_ICON_STYLE] == 1);
    shot17("home-engraved");
    tap17(500, 606);
    assert(app_for_route(ui_route()) == APP_FILE_TRANSFER);
    tap17(300,765);
    assert(ui_route() == 8);
    tap17(550, 96);
    assert(ui_route()==8 && strstr(app_process_frame(),"\"back\":true"));
    tap17(20, 20);
    assert(ui_route()==8 && strstr(app_process_frame(),"\"back\":false"));
    tap17(20, 20);
    assert(ui_route()==131);
    tap17(80,482);
    assert(ui_route() == app_definition(home_ids[0])->entry);
    tap17(20,52);
    assert(ui_route() == 131);
    route_to(13);
    ui_draw();
    tap17(530,134+3*72+30);
    assert(ui_route() == 83 && app_for_route(83) == APP_SETTINGS);
    tap17(20,20);
    assert(ui_route() == 13);
    route_to(9);
    setting_category = 1;
    ui_draw();
    tap17(490,150);
    assert(settings_ui.setting_page == 2);
    tap17(480,225);
    assert(dropdown && drop_count == 5 && drop_visible == 5);
    tap17(drop_x+70,drop_y+5+48+23);
    assert(options[OPT_LOCK] == 5 && !dropdown);
    route_to(1);
    ui_draw();
    tap17(550,83);
    assert(ui_route() == 132 && app_for_route(132) == APP_LIBRARY);

    route_to(30);
    open_keyboard(30,"Notiz","First line");
    ui_draw();
    assert(ui_pixels[78*WIDTH+180] == theme_gray(231));
    editor_ui.keyboard = 0;
    route_to(25);
    app_new(25);
    block_game.paused=0;
    app_next_tick=app_millis()+650;
    int falling_y=block_game.y;
    Input tick={.kind=INPUT_STATUS};
    for (int i=0;i<30;i++) {
        assert(apps_input(&tick)==2);
        assert(block_game.y==falling_y);
    }
    app_next_tick=app_millis()-1;
    assert(apps_input(&tick)==1 && block_game.y==falling_y+1);
    route_to(26);
    app_new(26);
    snake_game.paused=0;
    app_next_tick=app_millis()+550;
    int snake_head=snake_game.cells[0];
    for (int i=0;i<30;i++) {
        assert(apps_input(&tick)==2);
        assert(snake_game.cells[0]==snake_head);
    }
    app_next_tick=app_millis()-1;
    assert(apps_input(&tick)==1 && snake_game.cells[0]!=snake_head);
    char photo_file[2048];
    snprintf(photo_file,sizeof(photo_file),"%s/rotation-test.pgm",data_root);
    unsigned char *source=malloc(480000), *result=malloc(480000);
    assert(source && result);
    for (int i=0;i<480000;i++) source[i]=(unsigned char)(i*17+i/600);
    FILE *photo=fopen(photo_file,"wb");
    assert(photo);
    fputs("P5\n600 800\n255\n",photo);
    assert(fwrite(source,1,480000,photo)==480000);
    fclose(photo);
    for (int i=0;i<4;i++) assert(photo_image_rotate(photo_file));
    assert(photo_image_read(photo_file,result) && !memcmp(source,result,480000));
    free(source);free(result);unlink(photo_file);
    char *html=NULL;
    size_t html_size=0;
    FILE *md=open_memstream(&html,&html_size);
    markdown_render17(md,"**Bold** and *italic*.\n\n---\n\n- Outer\n  - Inner\n\n| A | B |\n|---|---|\n| 1 | 2 |\n");
    fclose(md);
    assert(strstr(html,"<strong>Bold</strong>") && strstr(html,"<em>italic</em>"));
    assert(strstr(html,"<hr") && strstr(html,"<table>") && strstr(html,"<ul>"));
    free(html);
    const char *previous_data_root=data_root;
    data_root="build/tests/default-photo";
    mkdir(data_root,0700);
    mkdir("build/tests/default-photo/photos",0700);
    unlink("build/tests/default-photo/photo-default-seeded");
    unlink("build/tests/default-photo/photos/LUMEN - Alpine Dawn.pgm");
    photo_seed_default();
    assert(!access("build/tests/default-photo/photos/LUMEN - Alpine Dawn.pgm",R_OK));
    unlink("build/tests/default-photo/photos/LUMEN - Alpine Dawn.pgm");
    photo_seed_default();
    assert(access("build/tests/default-photo/photos/LUMEN - Alpine Dawn.pgm",F_OK));
    data_root=previous_data_root;
    for(int face=0;face<CLOCK_FACE_COUNT;face++) {
        route_to(28);timer_panel=0;clock_ui.clock_style=face;ui_draw();
    }
    static unsigned char display_type[460*120];
    assert(ui_display_text("07:00",1,96,460,120,display_type));
    int type_ink=0;for(size_t i=0;i<sizeof(display_type);i++)type_ink+=display_type[i]<128;
    assert(type_ink>1000 && type_ink<20000);
    /* Fractions and all text operators now use the same measured math axis. */
    snprintf(calc_expr,sizeof(calc_expr),"5/4*sin(3/4)*sqrt(4)");
    mp=calc_expr;mn=math_bad=0;
    int math_root=math_expression(0,0);math_measure(math_root,0);
    assert(!math_bad && mb[math_root].up>0 && mb[math_root].down>0);
    rect(0,0,600,800,255);math_paint(math_root,28,300,0);
    assert(ui_pixels[300*600+32]==0); /* first fraction bar */
    int saved_note=note_selected;
    note_selected=ink_note=987654;extra_page=ink_page=0;
    memset(ink_pixels,255,sizeof(ink_pixels));ink_pixels[100*600+300]=0;
    editor_ui.keyboard=1;editor_ui.keyboard_target=30;editor_ui.note_view_start=2;
    rect(0,0,600,800,255);u15_note_overlay();
    assert(ui_pixels[(54+100-58)*600+300]==0);
    editor_ui.keyboard=0;ink_dirty=0;ink_note=ink_page=-1;note_selected=saved_note;
    route_to(131);
    settle_app();
    int call_depth=history_count;
    package_call_app(APP_MAZE,"show_help","",27);
    assert(ui_route()==119 && package_call_count==1 && history_count==call_depth+1);
    settle_app();
    tap17(20,20);
    assert(ui_route()==131 && !package_call_count && history_count==call_depth);
    char restart_dir[2048], restart_path[2048];
    snprintf(restart_dir,sizeof(restart_dir),"%s/../.update",data_root);
    mkdir(restart_dir,0700);
    snprintf(restart_path,sizeof(restart_path),"%s/relaunch",restart_dir);
    assert(atomic_write(restart_path,"1",1));
    Input network_event={.kind=INPUT_NETWORK};
    assert(ui_input(&network_event)==0);
    unlink(restart_path);
    puts("PASS delayed game ticks, lossless four-turn photo rotation and shared Markdown emphasis.");
    ui_close();
    engine_shutdown();
    puts("PASS Home Assistant UTF-8 device search, chat/archive paging and inline Reader action.");
    puts("PASS compact popups/nested folder menu, notes back, task editing/checkboxes, calculator "
         "dropdown, inline sleep, clipboard hit limits, warehouse/maze, crossword, journal "
         "editing, model list, Markdown Reader/chrome, local calendar and planet selector.");
}
