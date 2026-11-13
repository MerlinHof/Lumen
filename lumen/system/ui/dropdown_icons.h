/* Option semantics are shared across anchored menus and setting selectors. */
static int dropdown_option_icon(int target, int option, const char *label) {
    if (target == 902) return option < 10 ? IC_CLOCK : IC_DIGITAL;
    if (strstr(label,"Foto") || strstr(label,"Bild")) return IC_FIT;
    if (strstr(label,"Buch") || strstr(label,"Cover")) return IC_BOOK;
    if (strstr(label,"Zeit") || strstr(label,"Datum")) return IC_CLOCK;
    if (strstr(label,"Lösch")) return IC_TRASH;
    if (!strcmp(label,"Verschieben") || !strcmp(label,"Move") || !strcmp(label,"Déplacer")) return IC_MOVE;
    if (strstr(label,"Aus") || strstr(label,"Keine")) return IC_CLOSE;
    return IC_SETTINGS;
}
