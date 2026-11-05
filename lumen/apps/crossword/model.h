static int crossword_pick16;
static const char *cross_bank16[][2] = {
    {"PLANETS", "Welten um einen Stern (EN)"},
    {"READING", "Lesen (EN)"},
    {"SCIENCE", "Wissenschaft (EN)"},
    {"HISTORY", "Geschichte (EN)"},
    {"PHYSICS", "Physik (EN)"},
    {"WEATHER", "Wetter (EN)"},
    {"NATURAL", "Naturgegeben (EN)"},
    {"MEMOIRS", "Lebenserinnerungen (EN)"},
    {"SPACE", "Weltraum (EN)"},
    {"EARTH", "Unser Heimatplanet (EN)"},
    {"TREE", "Große Pflanze mit Stamm (EN)"},
    {"STONE", "Stück Gestein (EN)"},
    {"MOON", "Natürlicher Erdbegleiter (EN)"},
    {"SUN", "Stern unseres Sonnensystems (EN)"},
    {"WATER", "H2O (EN)"},
    {"FIRE", "Flammen (EN)"},
    {"BOOK", "Gebundene Seiten zum Lesen (EN)"},
    {"PAGE", "Seite eines Buchs (EN)"},
    {"INK", "Flüssiger Schreibstoff (EN)"},
    {"LIGHT", "Helligkeit (EN)"},
    {"DARK", "Ohne Licht (EN)"},
    {"GREEN", "Farbe des Grases (EN)"},
    {"BLUE", "Farbe eines klaren Himmels (EN)"},
    {"RED", "Farbe einer reifen Tomate (EN)"},
    {"WHITE", "Farbe von Schnee (EN)"},
    {"BLACK", "Dunkelste Farbe (EN)"},
    {"BRAIN", "Denkorgan (EN)"},
    {"HEART", "Blutpumpe im Körper (EN)"},
    {"NERVE", "Leitung im Nervensystem (EN)"},
    {"ATOM", "Baustein eines Elements (EN)"},
    {"ENERGY", "Fähigkeit, Arbeit zu verrichten (EN)"},
    {"FORCE", "Masse mal Beschleunigung (EN)"},
    {"MASS", "Physikalische Größe in Kilogramm (EN)"},
    {"TIME", "Größe in Sekunden (EN)"},
    {"WAVE", "Schwingung, die sich ausbreitet (EN)"},
    {"STAR", "Leuchtende Gaskugel am Himmel (EN)"},
    {"ORBIT", "Umlaufbahn (EN)"},
    {"NORTH", "Gegenüber von Süden (EN)"},
    {"SOUTH", "Gegenüber von Norden (EN)"},
    {"EAST", "Richtung des Sonnenaufgangs (EN)"},
    {"WEST", "Gegenüber von Osten (EN)"},
    {"RAIN", "Flüssiger Niederschlag (EN)"},
    {"SNOW", "Gefrorener Niederschlag (EN)"},
    {"WIND", "Bewegte Luft (EN)"},
    {"CLOUD", "Wassertröpfchen am Himmel (EN)"},
    {"RIVER", "Fließendes Gewässer (EN)"},
    {"OCEAN", "Großes Meer (EN)"},
    {"FOREST", "Viele Bäume zusammen (EN)"},
    {"LEAF", "Grüner Teil einer Pflanze (EN)"},
    {"ROOT", "Verankert die Pflanze im Boden (EN)"},
    {"SEED", "Aus ihm wächst eine Pflanze (EN)"},
    {"BIRD", "Tier mit Federn (EN)"},
    {"FISH", "Tier mit Kiemen (EN)"},
    {"HORSE", "Tier, auf dem man reiten kann (EN)"},
    {"MOUSE", "Kleines Nagetier (EN)"},
    {"HOUSE", "Gebäude zum Wohnen (EN)"},
    {"CHAIR", "Möbel zum Sitzen (EN)"},
    {"TABLE", "Möbel mit Platte (EN)"},
    {"TRAIN", "Fährt auf Schienen (EN)"},
    {"ROAD", "Verkehrsweg (EN)"},
    {"BRIDGE", "Bauwerk über einen Fluss (EN)"},
    {"MUSIC", "Kunst mit Tönen (EN)"},
    {"PIANO", "Instrument mit Tasten (EN)"},
    {"VOICE", "Klang beim Sprechen (EN)"},
    {"DREAM", "Erlebnis im Schlaf (EN)"},
    {"IDEA", "Einfall (EN)"},
    {"LOGIC", "Lehre vom folgerichtigen Denken (EN)"},
    {"NUMBER", "Mathematischer Zählbegriff (EN)"},
    {"CIRCLE", "Runde geometrische Form (EN)"},
    {"SQUARE", "Viereck mit vier gleichen Seiten (EN)"},
    {"SKY", "Himmel (EN)"},
    {"DAY", "Tag (EN)"},
    {"YEAR", "Jahr (EN)"},
    {"KEY", "Schlüssel (EN)"},
};
static const char *cross_words16[5], *cross_clues16[5];
static int cross_x16[5], cross_y16[5] = {1, 1, 3, 5, 7}, cross_down16[5] = {1, 0, 0, 0, 0};
static unsigned cross_seed16;
static void crossword_new16(unsigned seed) {
    cross_seed16 = seed ? seed : random16();
    unsigned state = game_seed16;
    game_seed16 = cross_seed16;
    int central = random16() % 8;
    cross_words16[0] = cross_bank16[central][0];
    cross_clues16[0] = cross_bank16[central][1];
    cross_x16[0] = 4;
    for (int i = 1; i < 5; i++) {
        int choices[200], columns[200], count = 0;
        char match = cross_words16[0][(i - 1) * 2];
        for (unsigned j = 8; j < sizeof(cross_bank16) / sizeof(*cross_bank16); j++)
            for (int k = 0; cross_bank16[j][0][k]; k++)
                if (cross_bank16[j][0][k] == match && k <= 4 &&
                    4 - k + (int)strlen(cross_bank16[j][0]) <= 10 && count < 200) {
                    choices[count] = j;
                    columns[count++] = 4 - k;
                }
        int pick = random16() % count;
        cross_words16[i] = cross_bank16[choices[pick]][0];
        cross_clues16[i] = cross_bank16[choices[pick]][1];
        cross_x16[i] = columns[pick];
    }
    for (int i = 0; i < 5; i++) {
        int language;
        const char *definition = dictionary_lookup(cross_words16[i], 1, &language);
        if (definition && language == options[OPT_LANGUAGE]) cross_clues16[i] = definition;
    }
    game_seed16 = state;
    memset(crossword_answer16, 0, sizeof(crossword_answer16));
    char b[40];
    snprintf(b, 40, "%u", cross_seed16);
    extra_write_text("crossword-seed16.txt", b);
    save_blob("crossword16.bin", crossword_answer16, sizeof(crossword_answer16));
}
static void crossword_draw16(void) {
    heading_actions("Kreuzworträtsel", 1);
    icon_button(528, 74, IC_RESTART);
    char grid[9][10] = {{0}};
    int mask[9][10] = {{0}};
    for (int i = 0; i < 5; i++)
        for (int k = 0; cross_words16[i][k]; k++) {
            int x = cross_x16[i] + (!cross_down16[i]) * k, y = cross_y16[i] + cross_down16[i] * k;
            mask[y][x] = 1;
            if (crossword_answer16[i][k])
                grid[y][x] = crossword_answer16[i][k];
        }
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 10; x++)
            if (mask[y][x]) {
                int xx = 30 + x * 54, yy = 132 + y * 46;
                key_outline(xx, yy, 54, 46);
                if (grid[y][x]) {
                    char b[2] = {grid[y][x], 0};
                    centered(xx, yy, 54, 46, b, ui_font(UI_BODY));
                }
            }
    for (int i = 0; i < 5; i++) {
        char b[260];
        snprintf(b, sizeof(b), "%d. %s%s", i + 1, cross_clues16[i],
                 !strcasecmp(crossword_answer16[i], cross_words16[i]) ? " ✓" : "");
        button_h(28, 520 + i * 49, 544, 43, b, 0);
    }
}
