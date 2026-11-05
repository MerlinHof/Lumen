/* App-owned module; assembled by apps/host/compose.h. */
static void hex_center(int i, int *x, int *y) {
    int r = i / 11, c = i % 11;
    *x = 60 + r * 48;
    *y = 155 + c * 36 + r * 18;
}
static void hex_shape(int x, int y, int fill) {
    for (int yy = -18; yy <= 18; yy++) {
        int w = 32 - abs(yy) * 16 / 18;
        rect(x - w, y + yy, 2 * w + 1, 1, fill);
    }
    int dx[] = {-32, -16, 16, 32, 16, -16}, dy[] = {0, -18, -18, 0, 18, 18};
    for (int i = 0; i < 6; i++)
        stroke(x + dx[i], y + dy[i], x + dx[(i + 1) % 6], y + dy[(i + 1) % 6], 2);
}
static void v1_hex_draw(void) {
    heading_actions("Hex", 1);
    icon_button(528, 74, IC_RESTART);
    centered(28, 128, 544, 38,
             hex_win ? (hex_win == 1 ? "Schwarz gewinnt" : "Grau gewinnt")
                     : (hex_turn == 1 ? "Schwarz · links nach rechts" : "Grau · oben nach unten"),
             ui_font(UI_BODY));
    for (int i = 0; i < 121; i++) {
        int x, y;
        hex_center(i, &x, &y);
        hex_shape(x, y, hex_board[i] == 1 ? 0 : hex_board[i] == 2 ? 161 : 255);
    }
    stroke(26, 155, 26, 515, 4);
    stroke(574, 335, 574, 695, 4);
    stroke(60, 133, 540, 313, 3);
    stroke(60, 537, 540, 717, 3);
    button_h(28, 735, 260, 48, "Zug zurück", 0);
    button_h(312, 735, 260, 48, hex_swap ? "Seiten tauschen" : "Neue Partie", 0);
}
static const char *v1_lessons[][3] = {
    {"Guten Morgen", "Good morning", "Bonjour"},
    {"Danke", "Thank you", "Merci"},
    {"Bitte", "Please", "S'il vous plaît"},
    {"Entschuldigung", "Excuse me", "Excusez-moi"},
    {"Wo ist der Bahnhof?", "Where is the station?", "Où est la gare ?"},
    {"Ich verstehe nicht", "I don't understand", "Je ne comprends pas"},
    {"Wasser", "Water", "Eau"},
    {"Buch", "Book", "Livre"},
    {"Heute", "Today", "Aujourd'hui"},
    {"Morgen", "Tomorrow", "Demain"},
    {"Ich möchte lernen", "I want to learn", "Je veux apprendre"},
    {"Wie viel kostet das?", "How much is it?", "Combien ça coûte ?"},
    {"Links", "Left", "Gauche"},
    {"Rechts", "Right", "Droite"},
    {"Guten Abend", "Good evening", "Bonsoir"},
    {"Auf Wiedersehen", "Goodbye", "Au revoir"},
    {"Ja", "Yes", "Oui"},
    {"Nein", "No", "Non"},
    {"Ich heiße …", "My name is …", "Je m'appelle …"},
    {"Wie geht es dir?", "How are you?", "Comment vas-tu ?"},
    {"Gut", "Good", "Bien"},
    {"Freund", "Friend", "Ami"},
    {"Zeit", "Time", "Temps"},
    {"Haus", "House", "Maison"}};
