#define _GNU_SOURCE
#include "../../lumen/system/ui/ui.c"
#include <assert.h>
static int action_x(const char *app,const char *intent) {
    for(int i=0;i<status_action_count;i++) {
        OverlayAction *action=&status_actions[i];
        if(!strcmp(action->app,app) && !strcmp(action->intent,intent) &&
           (*app || !strcmp(action->command,"battery")))
            return action->bounds.x+action->bounds.w/2;
    }
    return -1;
}
static void settle_app(void) {
    if(!app_package_installed(active_app()))return;
    app_process_open(active_app());
    for(int i=0;i<200 && (i<30 || !*app_process_frame());i++) {
        if(!app_package_installed(active_app()))break;
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
#include "../../lumen/system/power_state.h"
static void test_power_state(void) {
    assert(power_state_event("outOfScreenSaver 1")==POWER_AWAKE);
    assert(power_state_event("exitingScreenSaver")==POWER_AWAKE);
    assert(power_state_event("goingToScreenSaver 2")==POWER_ASLEEP);
    assert(power_state_event("other goingToScreenSaver")==POWER_UNKNOWN);
    assert(power_state_status("Powerd state: Active")==POWER_AWAKE);
    assert(power_state_status("Powerd state: Screen Saver")==POWER_ASLEEP);
    assert(power_state_status("")==POWER_UNKNOWN);
}
static void test_editor_feedback(void) {
    memset(ui_pixels,255,WIDTH*HEIGHT);
    package_paint_frame("{\"ui\":[{\"type\":\"text\",\"x\":40,\"y\":280,\"w\":510,\"h\":54,"
                        "\"role\":2,\"align\":\"right\",\"text\":\"123.45\"}]}");
    int left=WIDTH,right=0;
    for(int y=280;y<334;y++)for(int x=40;x<550;x++)
        if(ui_pixels[y*WIDTH+x]<200){if(x<left)left=x;if(x>right)right=x;}
    assert(left>350 && right>530 && right<550);
    package_caret_visible=1;
    caret_restart();
    assert(caret_wait()>0);
    editor_ui.cursor_epoch-=CARET_BLINK_MS;
    assert(caret_tick() && !editor_ui.cursor_visible);
    editor_ui.cursor_epoch-=CARET_BLINK_MS;
    assert(caret_tick() && editor_ui.cursor_visible);
    editor_ui.cursor_epoch-=CARET_BLINK_MS*CARET_PHASES;
    assert(caret_wait()==-1);
    package_caret_visible=0;
}
static void test_package_surfaces(void) {
    memset(ui_pixels,255,WIDTH*HEIGHT);
    package_paint_frame("{\"ui\":["
        "{\"type\":\"surface\",\"x\":30,\"y\":100,\"w\":80,\"h\":50,\"radius\":10,\"gray\":255,\"outline\":105},"
        "{\"type\":\"surface\",\"x\":120,\"y\":100,\"w\":80,\"h\":50,\"radius\":8,\"gray\":215,\"outline\":215},"
        "{\"type\":\"surface\",\"x\":210,\"y\":100,\"w\":80,\"h\":50,\"radius\":8,\"gray\":255,\"outline\":2}]}");
    assert(ui_pixels[125*WIDTH+30]==105 && ui_pixels[125*WIDTH+32]==255);
    assert(ui_pixels[125*WIDTH+120]==theme_gray(215) && ui_pixels[125*WIDTH+150]==theme_gray(215));
    assert(ui_pixels[125*WIDTH+210]==0 && ui_pixels[125*WIDTH+212]==255);
    widget_count=0;
    package_component_count=0;
    package_paint_frame("{\"ui\":[{\"type\":\"surface\",\"x\":30,\"y\":100,\"w\":84,\"h\":64,"
        "\"radius\":26,\"gray\":255},{\"type\":\"custom_action\",\"id\":\"card\",\"x\":30,\"y\":100,\"w\":84,\"h\":64}]}");
    assert(widget_count==1 && widgets[0].radius==26);
    char form[4096]="{\"ui\":[{\"type\":\"page_layout\",\"text\":\"Example\",\"scroll\":1}";
    for(int i=0;i<20;i++) {
        size_t used=strlen(form);
        snprintf(form+used,sizeof(form)-used,
            ",{\"type\":\"component_button\",\"id\":\"row-%d\",\"text\":\"Row\"}",i);
    }
    strcat(form,"]}");
    package_paint_frame(form);
    assert(package_form.pages>=3 && package_form.page==0);
    package_form.page=2;
    package_component_count=widget_count=0;
    package_paint_frame(form);
    assert(package_component_count>0 && !strcmp(package_components[0].id,"row-18"));
    package_form.active=0;

}
int main(int argc, char **argv) {
    (void)argv;
    library_root = "build/tests/fixtures";
    data_root = "build/tests/test-data";
    mkdir(data_root, 0700);
    assert(engine_init());
    test_power_state();
    test_package_surfaces();
    test_editor_feedback();
    assert(atomic_write("build/tests/test-data/photo-default-seeded","1",1));
    ui_init();
    for(int i=0;i<200 && !keyboard_layout_serial;i++){usleep(3000);keyboard_package_poll();}
    assert(keyboard_layout_serial);
    assert(!app_package_foreground(app_package_find("retired.rpg")));
    (void)argc;
    activate_app(app_package_find("lumen.periodic-table"));settle_app();ui_draw();
    assert(package_component_count==118);
    int element=-1;
    for(int i=0;i<package_component_count;i++)
        if(!strcmp(package_components[i].id,"element:117"))element=i;
    assert(element>=0);
    PackageComponent target=package_components[element];
    tap17(target.left+target.width/2,target.top+target.height/2);
    assert(strstr(app_process_frame(),"118  Og"));
    activate_app(app_package_find("lumen.notes"));settle_app();ui_draw();
    tap17(550,95);assert(active_app()==app_package_find("lumen.notes") && editor_ui.keyboard);
    strcpy(editor_ui.edit,"Title\nSome text to edit.");commit_keyboard();settle_app();ui_draw();
    assert(strstr(app_process_frame(),"Some text to edit."));
    tap17(490,78);
    assert(strstr(app_process_frame(),"pointer_area"));
    Input stroke_begin={.kind=INPUT_DOWN,.x=120,.y=210};
    Input stroke_end={.kind=INPUT_TAP,.x=220,.y=210};
    ui_input(&stroke_begin);ui_input(&stroke_end);
    settle_app();ui_draw();
    assert(ui_pixels[210*WIDTH+170]<50);
    tap17(300,80);
    assert(ui_pixels[210*WIDTH+170]==255);
    ui_input(&stroke_begin);ui_input(&stroke_end);
    settle_app();ui_draw();
    tap17(549,80);
    DIR *photos=opendir("build/tests/test-data/photos");
    assert(photos);
    int exported=0;
    struct dirent *photo_entry;
    while((photo_entry=readdir(photos))) {
        if(strncmp(photo_entry->d_name,"note-",5))continue;
        char path[1024];
        snprintf(path,sizeof(path),"%s/photos/%s",data_root,photo_entry->d_name);
        FILE *file=fopen(path,"rb");
        unsigned char photo[WIDTH*HEIGHT];
        assert(file && photo_image_read(path,photo));
        fclose(file);
        assert(photo[210*WIDTH+170]<50);
        assert(!unlink(path));
        exported=1;
    }
    closedir(photos);
    assert(exported);
    tap17(20,20);
    tap17(90,95);
    assert(editor_ui.keyboard);
    strcpy(editor_ui.edit,"Title\nEdited during an autosave.");
    ui_save();settle_app();
    assert(strstr(app_process_frame(),"Edited during an autosave."));
    commit_keyboard();settle_app();ui_draw();
    tap17(553,77);assert(active_app()==app_package_find("lumen.reader"));
    settle_app();ui_draw();
    if(!reader_ui.reader_chrome)tap17(300,70);
    tap17(20,20);
    assert(active_app()==app_package_find("lumen.notes") && strstr(app_process_frame(),"Edited during an autosave."));
    tap17(20,20);
    assert(active_app()==app_package_find("lumen.notes") && strstr(app_process_frame(),"Title"));
    shot17("notes");
    activate_app(app_package_find("lumen.settings"));settle_app();ui_draw();
    tap17(300,240); /* Appearance */
    tap17(543,361); /* Inverted display */
    assert(options[OPT_INVERT]==1);
    tap17(543,361);
    assert(options[OPT_INVERT]==0);
    activate_app(app_package_find("lumen.todo"));
    settle_app();
    ui_draw();
    tap17(550,92);
    assert(editor_ui.keyboard && editor_ui.keyboard_target==2000);
    strcpy(editor_ui.edit,"Task\nA second line");
    commit_keyboard();
    settle_app();
    ui_draw();
    assert(strstr(app_process_frame(),"A second line"));
    tap17(160,165);
    assert(editor_ui.keyboard && strstr(editor_ui.edit,"A second line"));
    strcpy(editor_ui.edit,"Edited task");
    commit_keyboard();
    settle_app();
    ui_draw();
    tap17(42,171);
    assert(strstr(app_process_frame(),"checked\":1"));
    shot17("todo");
    activate_app(app_package_find("lumen.calculator"));
    settle_app();
    ui_draw();
    assert(ui_pixels[245*WIDTH+28]==105 && ui_pixels[245*WIDTH+33]==255);
    tap17(550,95);
    shot17("calc-settings");
    tap17(400, 225);
    assert(dropdown && drop_target == 2001);
    tap17(drop_x + 80, drop_y + 5 + 48 + 15);
    assert(strstr(app_process_frame(),"RAD"));
    activate_app(app_package_find("lumen.settings"));settle_app();ui_draw();
    if(app_process_frame_back())tap17(20,20);
    tap17(530,235);tap17(490,150);tap17(450,525);
    assert(dropdown && drop_target==2001);
    dropdown=0;
    activate_app(app_package_find("lumen.notes"));settle_app();ui_draw();
    Input hold = {.kind = INPUT_HOLD, .x = 300, .y = 600};
    activate_app(app_package_find("lumen.calculator"));
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
    activate_app(app_package_find("lumen.sokoban"));
    settle_app();
    assert(strstr(app_process_frame(),"Sokoban"));
    ui_draw();
    shot17("sokoban");
    activate_app(app_package_find("lumen.maze"));
    settle_app();
    assert(strstr(app_process_frame(),"Maze") || strstr(app_process_frame(),"Labyrinth"));
    ui_draw();
    shot17("maze");
    activate_app(app_package_find("lumen.crossword"));
    settle_app();
    ui_draw();
    shot17("crossword");
    activate_app(app_package_find("lumen.journal"));
    settle_app();
    ui_draw();
    shot17("journal-list");
    tap17(150,165);
    tap17(150,250);
    assert(editor_ui.keyboard && editor_ui.keyboard_target==2002 && keyboard_multiline());
    snprintf(editor_ui.edit,sizeof(editor_ui.edit),"My day\nSecond line");
    commit_keyboard();
    settle_app();
    ui_draw();
    shot17("journal");
    char study_folder[1024],study_document[1024];
    snprintf(study_folder,sizeof(study_folder),"%s/study",data_root);
    snprintf(study_document,sizeof(study_document),"%s/study/scope-test.html",data_root);
    assert(!mkdir(study_folder,0700) || errno==EEXIST);
    const char *study_html="<html><body><p>Study fixture</p></body></html>";
    assert(atomic_write(study_document,study_html,strlen(study_html)));
    activate_app(app_package_find("lumen.study"));
    settle_app();
    assert(strstr(app_process_frame(),"scope-test.html"));
    assert(app_process_send("{\"event\":\"component\",\"id\":\"material-0\",\"gesture\":\"clicked\"}"));
    settle_app();
    assert(active_app()==app_package_find("lumen.reader") && reader_ui.reader_document_active && engine_count()>0);
    unlink(study_document);
    extra_write_text("llm-models17.json", "{\"data\":[{\"id\":\"model-b\"},{\"id\":\"model-a\"}]}");
    extra_write_text("llm-key.txt", "dummy-visible-key");
    activate_app(app_package_find("lumen.openai"));settle_app();ui_draw();
    tap17(550,95);assert(strstr(app_process_frame(),"form_choice"));shot17("llm-settings");
    tap17(20,20);
    shot17("llm");
    reader_markdown17("Markdown test", "# Heading\n\nPeople moved through town all day before dusk. Strange shadows gathered slowly as winter lengthened and lights appeared. Darcyville thrived. Mr. Darcy arrived silently. A **bold** paragraph. When Darcy returned, there were still good reasons to wait for the last train home. Afterward a visitor opened one window toward the quiet lane. Later footsteps echoed past the river.\n\n- One\n- Two\n", active_app());
    DocumentMention mentions[4];
    assert(engine_mentions("Darcy",0,mentions,4)==2);
    assert(strstr(mentions[0].quote,"Darcyville thrived. Mr. Darcy arrived silently."));
    assert(mentions[0].length==5 && !strncmp(mentions[0].quote+mentions[0].offset,"Darcy",5));
    assert(mentions[1].length==5 && strstr(mentions[1].quote,"Darcy returned"));
    assert(strstr(mentions[1].quote,"toward the quiet lane. Later footsteps ..."));
    assert(!strncmp(mentions[1].quote,"... town all day",16));
    assert(!strstr(mentions[1].quote,"echoed past"));
    assert(strstr(mentions[0].quote,"paragraph. When Darcy returned,"));
    assert(!strncmp(mentions[0].quote,"... ",4) &&
        !strcmp(mentions[0].quote+strlen(mentions[0].quote)-4," ..."));
    assert(engine_mentions("Head",0,mentions,4)==0);
    assert(active_app()==app_package_find("lumen.reader") && reader_ui.reader_document_active);
    settle_app();ui_draw();
    snprintf(reader_ui.selection.text,sizeof(reader_ui.selection.text),"Darcy");
    reader_ui.selecting=2;
    assert(app_process_send("{\"event\":\"intent\",\"operation\":\"document-selection\",\"text\":\"Darcy\",\"top\":300,\"bottom\":330}"));
    settle_app();ui_draw();
    assert(strstr(app_process_frame(),"selection-copy"));
    tap17(100,409);
    assert(!strcmp(clipboard_text,"Darcy") && reader_ui.selecting==0);
    notification_dismiss();
    assert(app_process_send("{\"event\":\"intent\",\"operation\":\"selection-context\",\"text\":\"Darcy\"}"));
    settle_app();
    for(int i=0;i<25 && document_excerpt_wait()>=0;i++){usleep(10000);document_excerpt_poll();}
    settle_app();
    assert(strstr(app_process_frame(),"context-close") && strstr(app_process_frame(),"\"count\":2") && !strstr(app_process_frame(),"context-passage") && !strstr(app_process_frame(),"Seite 1"));
    assert(app_process_send("{\"event\":\"service\",\"action\":103,\"ok\":1,\"data\":\"Mr. Darcy\\nA character in Pride and Prejudice.\"}"));
    settle_app();
    assert(strstr(app_process_frame(),"A character in Pride and Prejudice."));
    shot17("reader-context");
    tap17(367,520);
    assert(strstr(app_process_frame(), "Darcy returned"));
    tap17(541,181);
    tap17(300,70);
    assert(reader_ui.reader_chrome);shot17("reader");
    int page_before=page;
    tap17(300,300);
    assert(!reader_ui.reader_chrome && page==page_before);
    tap17(300,70);assert(reader_ui.reader_chrome);
    tap17(550,95);assert(active_app()==app_package_find("lumen.reader") && strstr(app_process_frame(),"Schriftfamilie"));
    tap17(314,766);
    assert(strstr(app_process_frame(),"Lesefortschritt"));
    tap17(480,245);
    assert(dropdown && drop_target==2001);
    dropdown = 0;
    activate_app(app_package_find("lumen.calendar"));
    settle_app();
    ui_draw();
    tap17(181,92);
    assert(strstr(app_process_frame(),"selected\":1"));
    shot17("calendar-week");
    tap17(200,350);
    assert(strstr(app_process_frame(),"selected\":0"));
    shot17("calendar-day");
    activate_app(app_package_find("lumen.planetarium"));
    settle_app();
    ui_draw();
    assert(strstr(app_process_frame(),"Sonnensystem"));
    tap17(450, 90);
    assert(strstr(app_process_frame(),"selected\":1"));
    assert(strstr(app_process_frame(),"Neptun"));
    shot17("solar");
    for (int lang = 0; lang < 3; lang++) {
        options[OPT_LANGUAGE] = lang;
        activate_app(app_package_find("lumen.openai"));
        ui_draw();
        shot17(lang == 0 ? "de" : lang == 1 ? "en" : "fr");
        assert(strcmp(tr_language("Dieser Ordner ist leer.", lang),
                      lang ? "Dieser Ordner ist leer." : "English") != 0);
    }
    options[OPT_LANGUAGE] = 0;
    input_tests18();
    improvements_tests();
    quality_tests();
    activate_app(app_package_find("lumen.frog-solitaire"));
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
    activate_app(app_package_find("lumen.clock"));
    settle_app();
    ui_draw();
    tap17(550, 159);
    assert(package_fullscreen);
    tap17(300, 400);
    assert(!package_fullscreen);
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
    assert(app_send(&(AppIntent){.app=app_package_find("lumen.photos"),.kind=INTENT_OPEN}));
    notification_dismiss();settle_app();ui_draw();
    assert(photo_count==2);
    tap17(549,96);assert(strstr(app_process_frame(),"Dauer pro Foto"));
    tap17(20,20);assert(active_app()==app_package_find("lumen.photos") && !strstr(app_process_frame(),"Dauer pro Foto"));
    tap17(430,96);
    assert(editor_ui.keyboard && editor_ui.keyboard_target==2000);
    strcpy(editor_ui.edit,"Test folder");commit_keyboard();settle_app();ui_draw();
    assert(strstr(app_process_frame(),"Test folder") && !strstr(app_process_frame(),"\"type\":\"photo\""));
    tap17(100,156);tap17(drop_x+85,drop_y+25);
    assert(photo_count==2 && strstr(app_process_frame(),"Alle Fotos"));
    Input photo_hold={.kind=INPUT_HOLD,.x=80,.y=240};
    ui_input(&photo_hold);settle_app();ui_draw();
    assert(dropdown && drop_target==2001);
    tap17(drop_x+85,drop_y+25);assert(dropdown && drop_target==2001);
    tap17(drop_x+85,drop_y+25);assert(!dropdown && photo_count==2);
    tap17(100,156);tap17(drop_x+85,drop_y+73);
    assert(strstr(app_process_frame(),"Test folder") && strstr(app_process_frame(),"\"type\":\"photo\""));
    tap17(450,156);assert(active_app()==app_package_find("lumen.photos") && package_fullscreen);
    app_process_send("{\"event\":\"tick\"}");settle_app();
    tap17(300,300);assert(active_app()==app_package_find("lumen.photos") && !package_fullscreen && strstr(app_process_frame(),"Test folder"));
    activate_app(app_package_find("lumen.insights"));settle_app();ui_draw();
    assert(ui_pixels[250*WIDTH+28]==115 && ui_pixels[250*WIDTH+34]==255);
    activate_app(app_package_find("lumen.photos"));settle_app();
    options[OPT_DATE] = options[OPT_CLOCK] = 1;
    ui_draw();
    for(int i=0;i<100 && action_x("lumen.calendar","")<0;i++){usleep(3000);status_overlay_poll();ui_draw();}
    assert(action_x("lumen.calendar","")>=0 && action_x("lumen.clock","clock-face")>=0);
    tap17(action_x("lumen.clock","clock-face"), 20);
    assert(active_app()==app_package_find("lumen.clock") && strstr(app_process_frame(),"selected\":0"));
    tap17(480,95);
    assert(strstr(app_process_frame(),"\"selected\":2") &&
           ui_pixels[400*WIDTH+28]==105 && ui_pixels[400*WIDTH+34]==255);
    tap17(20, 20);
    assert(active_app()==app_package_find("lumen.photos"));
    tap17(action_x("lumen.calendar",""), 20);
    assert(active_app()==app_package_find("lumen.calendar"));
    tap17(20, 20);
    assert(active_app()==app_package_find("lumen.photos"));

    assert(app_send(&(AppIntent){.app=app_package_find("lumen.home-assistant"),.kind=INTENT_OPEN}));
    settle_app();
    tap17(550,95);
    int ha_depth = history_count;
    ui_draw();
    tap17(20, 20);
    assert(active_app()==app_package_find("lumen.home-assistant") && !strstr(app_process_frame(),"back\":true") && history_count == ha_depth);
    tap17(20, 20);
    assert(active_app()==app_package_find("lumen.photos"));

    options[OPT_BATTERY] = 1;
    ui_draw();
    for(int i=0;i<100 && action_x("","")<0;i++){usleep(3000);status_overlay_poll();ui_draw();}
    assert(action_x("","")>=0);
    tap17(action_x("",""),20);
    assert(battery_popup);
    tap17(549, 96);
    assert(!battery_popup && active_app()==app_package_find("lumen.photos") && !strstr(app_process_frame(),"Dauer pro Foto"));

    memset(ui_pixels, 255, sizeof(ui_pixels));
    options[OPT_ROUNDING] = 1;
    widget_count = 0;
    button_h(20, 20, 100, 48, "", 0);
    unsigned char rim = ui_pixels[21*WIDTH+21];
    pressed = 0;
    press_bounds=widgets[pressed];
    ui_feedback_draw();
    assert(ui_pixels[21*WIDTH+21]==rim && ui_pixels[24*WIDTH+70]==0);
    assert(ui_pixels[42*WIDTH+24]==0 && ui_pixels[42*WIDTH+115]==0);
    assert(ui_pixels[42*WIDTH+70]==0);
    pressed = -1;
    memset(ui_pixels, 255, sizeof(ui_pixels));
    control_surface(20, 20, 48, 48, 16, 0);
    assert(ui_pixels[20*WIDTH+20] > 0);
    options[OPT_ROUNDING] = 0;
    memset(ui_pixels, 255, sizeof(ui_pixels));
    widget_count = 0;
    region(20, 60, 100, 48);
    pressed = 0;
    press_bounds = widgets[pressed];
    ui_feedback_draw();
    assert(ui_pixels[62*WIDTH+22] == 255 && ui_pixels[62*WIDTH+70] == 0);
    pressed = -1;
    control_surface(20, 20, 48, 48, 16, 0);
    assert(ui_pixels[20*WIDTH+20] == 0);
    options[OPT_ROUNDING] = 1;
    activate_app(app_package_find("lumen.home"));
    options[OPT_DATE] = options[OPT_CLOCK] = options[OPT_WIFI] = 1;
    assert(ui_prepare_home());
    ui_draw();
    for(int i=0;i<100 && action_x("lumen.calendar","")<0;i++){usleep(3000);status_overlay_poll();ui_draw();}
    assert(action_x("lumen.calendar","") < action_x("lumen.clock","clock-face") &&
           action_x("lumen.clock","clock-face") < action_x("lumen.settings","wifi"));
    assert(package_pager.count>1 && package_pager.page==0);
    int home_dots = pager_dot_start(PAGER_X,PAGER_W,package_pager.count);
    tap17(home_dots+45, 764);
    assert(package_pager.page==1);
    tap17(home_dots+15, 764);
    assert(package_pager.page==0);
    tap17(300,300);
    assert(active_app()==app_package_find("lumen.home") && package_pager.page==0);
    int home_ids[APP_LIMIT];
    int visible=v1_visible(home_ids);
    assert(visible>8);
    static const char *const first[]={"lumen.library","lumen.calculator","lumen.notes","lumen.calendar",
        "lumen.clock","lumen.todo","lumen.settings","lumen.transfer"};
    for(size_t i=0;i<sizeof(first)/sizeof(*first);i++)
        assert(!strcmp(app_package_key(home_ids[i]),first[i]));
    int notifications=app_package_find("lumen.notifications");
    assert(notifications>=0 && !launcher_visible[notifications]);
    assert(app_package_installed(app_package_find("lumen.home")) && app_package_installed(app_package_find("lumen.transfer")));
    assert(options[OPT_APP_ICON_STYLE] == 1);
    shot17("home-engraved");
    tap17(500, 606);
    assert(active_app() == app_package_find("lumen.transfer"));
    tap17(300,765);
    assert(active_app()==app_package_find("lumen.transfer"));
    tap17(550, 96);
    assert(active_app()==app_package_find("lumen.transfer") && strstr(app_process_frame(),"\"back\":true"));
    tap17(20, 20);
    assert(active_app()==app_package_find("lumen.transfer") && strstr(app_process_frame(),"\"back\":false"));
    tap17(20, 20);
    assert(active_app()==app_package_find("lumen.home"));
    tap17(80,482);
    assert(active_app() == home_ids[0]);
    tap17(20,52);
    assert(active_app()==app_package_find("lumen.home"));
    activate_app(app_package_find("lumen.settings"));settle_app();ui_draw();
    tap17(530,134+3*72+30);
    assert(active_app()==app_package_find("lumen.settings") && strstr(app_process_frame(),"Apps verwalten"));
    tap17(20,20);assert(strstr(app_process_frame(),"Einstellungen"));
    tap17(530,235);tap17(490,150);
    assert(strstr(app_process_frame(),"Automatisch sperren"));
    tap17(480,225);
    assert(dropdown && drop_count==5 && drop_visible==5);
    tap17(drop_x+70,drop_y+5+48+23);
    assert(options[OPT_LOCK]==5 && !dropdown);
    memset(ui_pixels,255,WIDTH*HEIGHT);
    dropdown_button(360,203,210,48,"Example");
    assert(ui_pixels[230*WIDTH+549]<100); /* shared selector chevron */
    activate_app(app_package_find("lumen.library"));
    settle_app();ui_draw();
    tap17(550,83);
    assert(active_app()==app_package_find("lumen.library") && strstr(app_process_frame(),"Sortierung"));

    Input sleep_fixture={.kind=INPUT_SLEEP},wake_fixture={.kind=INPUT_WAKE};
    Input release_fixture={.kind=INPUT_POWER};
    assert(ui_input(&sleep_fixture)==1 && sleeping);
    assert(ui_input(&sleep_fixture)==2 && sleeping);
    assert(ui_input(&release_fixture)==2 && sleeping);
    assert(ui_input(&wake_fixture)==1 && !sleeping);
    assert(ui_input(&sleep_fixture)==1 && ui_input(&wake_fixture)==1 && !sleeping);

    activate_app(app_package_find("lumen.library"));
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
    static unsigned char display_type[460*120];
    assert(ui_display_text("07:00",1,96,460,120,display_type));
    int type_ink=0;for(size_t i=0;i<sizeof(display_type);i++)type_ink+=display_type[i]<128;
    assert(type_ink>1000 && type_ink<20000);
    /* Fractions and all text operators now use the same measured math axis. */
    mp="5/4*sin(3/4)*sqrt(4)";mn=math_bad=0;
    int math_root=math_expression(0,0);math_measure(math_root,0);
    assert(!math_bad && mb[math_root].up>0 && mb[math_root].down>0);
    rect(0,0,600,800,255);math_paint(math_root,28,300,0);
    assert(ui_pixels[300*600+32]==0); /* first fraction bar */
    activate_app(app_package_find("lumen.home"));
    settle_app();
    int call_depth=history_count;
    package_call_app(app_package_find("lumen.maze"),"show_help","",27);
    assert(active_app()==app_package_find("lumen.maze") && package_call_count==1 && history_count==call_depth+1);
    settle_app();
    tap17(20,20);
    assert(active_app()==app_package_find("lumen.home") && !package_call_count && history_count==call_depth);
    activate_app(app_package_find("lumen.braintrain"));
    settle_app();
    ui_draw();
    assert(strstr(app_process_frame(),"exercise1"));
    tap17(400,275);
    assert(!strstr(app_process_frame(),"exercise1") && strstr(app_process_frame(),"back\":true"));
    activate_app(app_package_find("lumen.languages"));
    settle_app();
    ui_draw();
    tap17(310,150);
    assert(strstr(app_process_frame(),"selected\":1"));
    int old_lock=options[OPT_LOCK],old_date=options[OPT_LOCK_DATE];
    options[OPT_LOCK]=2;options[OPT_LOCK_DATE]=1;
    sleeping=1;ui_draw();
    for(int i=0;i<200 && !*app_overlay_frame(app_provider_overlay_slot(PROVIDER_SLEEP));i++) {
        usleep(5000);
        app_overlay_poll(app_provider_overlay_slot(PROVIDER_SLEEP));
    }
    assert(app_package_installed(app_package_find("lumen.lock-screen")));
    assert(strstr(app_overlay_frame(app_provider_overlay_slot(PROVIDER_SLEEP)),"display_text\",\"x\":20,\"y\":285"));
    assert(strstr(app_overlay_frame(app_provider_overlay_slot(PROVIDER_SLEEP)),"display_text\",\"x\":20,\"y\":418"));
    sleeping=0;options[OPT_LOCK]=old_lock;options[OPT_LOCK_DATE]=old_date;
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
