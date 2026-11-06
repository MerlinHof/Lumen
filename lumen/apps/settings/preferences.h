/* App-owned module; assembled by apps/host/compose.h. */
static const char *categories[] = {"Schrift & Satz",    "Darstellung", "Bibliothek",
                                   "Blättern & Gesten", "Netzwerk",    "System"};
static const char *fonts[] = {"Charis", "Sans Serif", "Monospace", "Times", "Noto Serif"};
static const char *alignments[] = {"Linksbündig", "Blocksatz"};
static const char *tapzones[] = {"Links / rechts", "Mehr Platz vorwärts"};
static const char *lockmodes[] = {"Buchcover", "Buchcover · Vollbild", "Uhrzeit",
                                  "Ruhiger Text", "Eigenes Bild", "Fotos · stündlich"};
static const int lock_mode_ids[6] = {0,5,2,1,3,4};
static int lock_mode_row(void) {
    for (int i=0;i<6;i++) if (lock_mode_ids[i] == options[OPT_LOCK]) return i;
    return 0;
}
static const char *languages[] = {"Deutsch", "English", "Français"};
typedef struct {
    const char *label;
    int category, key, min, max, step;
    const char **choices;
    int count;
} SettingRow;
static SettingRow setting_rows[] = {
    {"Schriftfamilie", 0, OPT_FONT, 0, 4, 1, fonts, 5},
    {"Ausrichtung", 0, OPT_ALIGN, 0, 1, 1, alignments, 2},
    {"Zeichenabstand", 0, OPT_LETTERSPACE, 0, 20, 2, NULL, 0},
    {"Absatzabstand", 0, OPT_PARAGRAPH, 0, 24, 4, NULL, 0},
    {"Sprache / Language", 1, OPT_LANGUAGE, 0, 2, 1, languages, 3},
    {"Tastatursprache", 1, OPT_KEYBOARD_LANGUAGE, 0, 2, 1, languages, 3},
    {"Datum anzeigen", 1, OPT_DATE, 0, 1, 1, NULL, 0},
    {"Uhrzeit anzeigen", 1, OPT_CLOCK, 0, 1, 1, NULL, 0},
    {"WLAN-Status anzeigen", 1, OPT_WIFI, 0, 1, 1, NULL, 0},
    {"Akkusymbol", 1, OPT_BATTERY, 0, 1, 1, NULL, 0},
    {"Akku-Prozentzahl", 1, OPT_BATTERY_PERCENT, 0, 1, 1, NULL, 0},
    {"Invertierte Darstellung", 1, OPT_INVERT, 0, 1, 1, NULL, 0},
    {"Cover zuschneiden", 1, OPT_COVER_CROP, 0, 1, 1, NULL, 0},
    {"Abgerundete Ecken", 1, OPT_ROUNDING, 0, 1, 1, NULL, 0},
    {"Sperrbild", 1, OPT_LOCK, 0, 5, 1, lockmodes, 6},
    {"Sperrbild-Hinweis", 1, OPT_LOCK_HINT, 0, 1, 1, NULL, 0},
    {"Wischen zum Blättern", 3, OPT_SWIPE, 0, 1, 1, NULL, 0},
    {"Tippzonen", 3, OPT_TAP, 0, 1, 1, tapzones, 2},
    {"Connect-Port", 4, OPT_PORT, 1024, 65535, 1, NULL, 0},
    {"Inaktivität (Minuten)", 4, OPT_TIMEOUT, 1, 60, 1, NULL, 0},
};
static const char *settings_sections[] = {"WLAN", "Darstellung", "Datum & Uhrzeit", "Apps verwalten", "System"};
static const char *settings_hints[] = {
    "Netzwerke und Verbindung",
    "Sprache, Sperrbild und Statusleiste", "Zeit, Zeitzone und WLAN-Abgleich",
    "Sichtbarkeit & Reihenfolge", "Speicher, Version und Konfiguration"};
static void settings_home(void) {
    heading("Einstellungen");
    for (int i = 0; i < 5; i++) {
        int y = 134 + i * 72;
        ListLayout l = list_row(28, y, 544, 72, -1, 62, tr(settings_sections[i]),
                                tr(settings_hints[i]), ui_font(UI_PRIMARY), 1);
        icon(526, l.action_y, IC_NEXT);
    }
}
