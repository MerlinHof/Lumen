/* Option semantics are shared across anchored menus and setting selectors. */
static int dropdown_option_icon(int target, int option, const char *label) {
    if (target == 940) return option==0 ? IC_FOLDER : IC_LOCK;
    if (target == 941) return IC_FIT;
    if (target == 942) return IC_ROTATE_PHOTO;
    if (target == 903 || target == 904) return IC_FOLDER;
    if (target == 905) return IC_CLOCK;
    if (target == 902) return option < 10 ? IC_CLOCK : IC_DIGITAL;
    if (target == 901) return option == 0 ? IC_PIN : IC_TRASH;
    if (target == 932) return option == 0 ? IC_MOVE : IC_TRASH;
    if (target == 936) return IC_FOLDER;
    if (target == 918) return IC_FLAG;
    if (target == 900 || target == 0) return IC_CONTENTS;
    if (target == 917 || target == 100 + OPT_TIMEOUT || target == 100 + OPT_ZONE)
        return IC_CLOCK;
    if (target == 920) return option == 0 ? IC_TODAY : IC_GRID;
    if (target == 933) return option == 0 ? IC_SEARCH : IC_GRAPH;
    if (target == 931 || target == 919) return IC_GRAPH;
    if (target == 916) {
        static const int icons[] = {IC_BOOK, IC_PREV, IC_PERCENT, IC_CONTENTS, IC_CLOCK, IC_CLOSE, IC_BARS};
        return icons[option >= 0 && option < 7 ? option : 0];
    }
    if (target == 100 + OPT_ALIGN) return IC_ALIGN;
    if (target == 100 + OPT_LANGUAGE || target == 100 + OPT_KEYBOARD_LANGUAGE) return IC_GLOBE;
    if (target == 100 + OPT_FONT || target == 100 + OPT_ALIGN ||
        target == 100 + OPT_LANGUAGE || target == 100 + OPT_KEYBOARD_LANGUAGE)
        return IC_TYPE;
    if (strstr(label,"Foto") || strstr(label,"Bild")) return IC_FIT;
    if (strstr(label,"Buch") || strstr(label,"Cover")) return IC_BOOK;
    if (strstr(label,"Zeit") || strstr(label,"Datum")) return IC_CLOCK;
    if (strstr(label,"Lösch")) return IC_TRASH;
    if (strstr(label,"Aus") || strstr(label,"Keine")) return IC_CLOSE;
    return IC_SETTINGS;
}
