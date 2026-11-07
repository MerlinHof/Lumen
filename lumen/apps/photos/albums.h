static void photo_create_folder(const char *name) {
    if (!*name) return;
    photo_catalog_load();
    int empty = -1;
    for (int i = 0; i < PHOTO_FOLDERS; i++) {
        if (!strcmp(photo_catalog.folders[i], name)) {
            ui_message("Dieser Ordner existiert bereits.");
            return;
        }
        if (!photo_catalog.folders[i][0] && empty < 0) empty = i;
    }
    if (empty < 0) {
        ui_message("Maximal 16 Fotoordner.");
        return;
    }
    snprintf(photo_catalog.folders[empty], sizeof(photo_catalog.folders[empty]), "%s", name);
    if (!photo_catalog_save()) {
        photo_catalog.folders[empty][0] = 0;
        return;
    }
    photo_folder = empty + 1;
    photo_page = 0;
    v1_photos_scan();
}
static void photo_open_folder_menu(int assign, int index) {
    static const char *labels[PHOTO_FOLDERS + 1];
    static char names[PHOTO_FOLDERS][72];

    int count = 0, selected_folder = 0;
    if (!assign) {
        labels[count] = "Alle Fotos";
        photo_menu_ids[count++] = 0;
    } else {
        if (index < 0 || index >= photo_count) return;
        photo_assignment = photo_id(photo_names[index]);
    }
    PhotoMembership *member = assign ? photo_membership(photo_assignment, 0) : NULL;
    for (int i = 0; i < PHOTO_FOLDERS; i++) {
        if (!photo_catalog.folders[i][0]) continue;
        photo_menu_ids[count] = i + 1;
        snprintf(names[i], sizeof(names[i]), "%s%s",
                 member && (member->folders & (1u << i)) ? "✓ " : "", photo_catalog.folders[i]);
        labels[count] = names[i];
        if (photo_folder == i + 1) selected_folder = count;
        count++;
    }
    if (!count) {
        ui_message("Erstelle zuerst einen Fotoordner.");
        return;
    }
    photo_menu_count = count;
    open_dropdown(assign ? 904 : 903, labels, count, assign ? -1 : selected_folder,
                  28, assign ? 188 : 181, 330);
}
static void photo_folder_choice(int target, int choice) {
    if (choice < 0 || choice >= photo_menu_count) return;
    int id = photo_menu_ids[choice];
    if (target == 903) {
        photo_folder = id;
        photo_page = 0;
        v1_photos_scan();
        return;
    }
    if (!id || !photo_assignment) return;
    PhotoMembership *item = photo_membership(photo_assignment, 1);
    if (!item) {
        ui_message("Zu viele Fotozuordnungen.");
        return;
    }
    PhotoMembership before = *item;
    item->id = photo_assignment;
    item->folders ^= 1u << (id - 1);
    if (!item->folders) item->id = 0;
    if (!photo_catalog_save()) *item = before;
    if (ui_route() == 72 && id == photo_folder && !(item->folders & (1u << (id - 1)))) {
        photo_page = 0;
        v1_photos_scan();
        extra_return(71);
    } else if (ui_route() == 71) {
        v1_photos_scan();
    }
}

static int photo_action_index;
static void photo_actions_open(int index) {
    static const char *actions[]={"Zu Ordner hinzufügen","Als Sperrbild verwenden"};
    photo_action_index=index;
    open_dropdown(940,actions,2,-1,28,188,400);
}
static void photo_set_lock(void) {
    if (photo_action_index<0 || photo_action_index>=photo_count) return;
    char name[256],path[2048];
    snprintf(name,sizeof(name),"%s",photo_names[photo_action_index]);
    snprintf(path,sizeof(path),"%s/lock-photo.txt",data_root);
    if (atomic_write(path,name,strlen(name))) {
        options[OPT_LOCK]=3;
        library_options_save();
        ui_message("Als Sperrbild gespeichert.");
    }
}

static void photo_actions_choice(int choice) {
    if (!choice) photo_open_folder_menu(1,photo_action_index);
    else photo_set_lock();
}

static void photo_setting_choice(int target,int choice) {
    int *value=target==941 ? &photo_catalog.fill : &photo_catalog.landscape;
    int previous=*value;
    *value=choice;
    if (!photo_catalog_save()) *value=previous;
}
