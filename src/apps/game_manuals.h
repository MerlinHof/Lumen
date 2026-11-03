static const struct {
    int route;
    const char *text[3];
} manuals17[] = {
    {23,
     {"Nutze Zahlen als genaue Minenzähler. Eine Flagge ist eine Vermutung. Wenn alle Minen einer "
      "Zahl bekannt sind, bleiben ihre übrigen Nachbarn sicher.",
      "Reveal every safe cell. Numbers count mines in the eight neighboring cells. Hold a cell to "
      "flag it; flags are hypotheses, not proof. Revealing a mine loses. If all mines around a "
      "number are identified, its other neighbors are safe. Start at open borders and prefer "
      "certain deductions over guesses.",
      "Découvrez toutes les cases sûres. Les chiffres comptent les mines parmi les huit voisines. "
      "Maintenez une case pour la marquer ; un drapeau reste une hypothèse. Une mine découverte "
      "termine la partie. Si toutes les mines autour d’un chiffre sont connues, ses autres "
      "voisines sont sûres. Préférez les déductions aux suppositions."}},
    {24,
     {"Beginne mit häufigen Vokalen. Nutze die Wortlänge und bestätigte Buchstaben, um passende "
      "Wörter einzugrenzen.",
      "Guess the hidden word by choosing letters. Correct letters appear in every matching "
      "position; each wrong letter adds a part to the hanging figure. Complete the word before the "
      "figure is finished. After a loss the answer is revealed. Start with common vowels and "
      "consonants, then use word length and known letters to narrow the possibilities.",
      "Devinez le mot en choisissant des lettres. Les lettres correctes apparaissent à toutes "
      "leurs positions ; chaque erreur complète le pendu. Trouvez le mot avant la fin du dessin. "
      "La solution est révélée après une défaite. Commencez par des voyelles et consonnes "
      "fréquentes, puis utilisez la longueur et les lettres connues."}},
    {25,
     {"Halte den Stapel flach. Überdeckte Löcher bleiben blockiert, bis die Reihen darüber "
      "verschwinden. Positioniere Steine auf E-Ink frühzeitig.",
      "Move and rotate falling pieces. A full horizontal row disappears. The game ends when new "
      "pieces no longer fit. Keep the stack fairly flat and avoid covered holes, which cannot be "
      "filled until rows above are removed. Leave a well for a long piece when useful. On E-Ink, "
      "position pieces early instead of relying on last-second corrections.",
      "Déplacez et tournez les pièces. Une ligne horizontale complète disparaît. La partie finit "
      "quand les nouvelles pièces ne peuvent plus entrer. Gardez une surface assez plane et évitez "
      "les trous recouverts. Conservez au besoin un puits pour une pièce longue. Sur E-Ink, placez "
      "les pièces tôt plutôt que de corriger au dernier instant."}},
    {26,
     {"Plane auch den Weg nach der nächsten Mahlzeit. Große Schleifen erhalten Platz und vermeiden "
      "Sackgassen.",
      "Guide the snake to food. Each meal makes it longer. Avoid walls and its own body; direct "
      "reversal is not allowed. Plan both the next meal and a route back into open space. Wide "
      "loops help preserve room. Change direction early, before the head reaches an obstacle.",
      "Guidez le serpent vers la nourriture. Chaque repas l’allonge. Évitez les murs et son corps "
      "; un demi-tour immédiat est impossible. Prévoyez le repas suivant et une sortie vers un "
      "espace libre. Les grandes boucles conservent de la place. Anticipez les virages."}},
    {48,
     {"Entwickle Figuren, kontrolliere das Zentrum und schütze deinen König. Ein Schachangriff "
      "muss sofort abgewehrt werden.",
      "Win by checkmate: attack the opposing king so it has no legal escape. Select a piece and "
      "its destination. Solo plays against the computer; multiplayer alternates on this device. "
      "Pawns advance and capture diagonally, rooks move straight, bishops diagonally, queens both, "
      "knights in an L and kings one square. Develop pieces, protect your king and control the "
      "center. Rotation turns artwork toward the other player without exchanging colors.",
      "Gagnez par échec et mat : le roi attaqué ne peut plus échapper légalement. Choisissez une "
      "pièce puis sa destination. Le mode solo affronte l’ordinateur ; le multijoueur alterne sur "
      "cet appareil. Les pions prennent en diagonale, les tours suivent les lignes, les fous les "
      "diagonales, les dames les deux, les cavaliers sautent en L et les rois avancent d’une case. "
      "Développez les pièces et protégez votre roi."}},
    {49,
     {"Suche vor jedem Zug erst einen eigenen Gewinn und dann eine gegnerische Drohung. Eine Gabel "
      "erzeugt zwei gleichzeitige Gewinnmöglichkeiten.",
      "Two players mark empty cells in turn. Three marks in a row, column or diagonal win. A full "
      "board without a line is a draw. First look for your own winning move, then block an "
      "immediate opposing win. A fork creates two threats at once, so the opponent can stop only "
      "one.",
      "Deux joueurs marquent une case vide à tour de rôle. Trois signes en ligne, colonne ou "
      "diagonale gagnent. Un plateau plein sans alignement est nul. Cherchez d’abord un coup "
      "gagnant, puis bloquez une menace immédiate. Une fourchette crée deux menaces simultanées."}},
    {63,
     {"Halte die größte Kachel in einer Ecke und ordne daneben absteigende Werte an. Vermeide "
      "unnötige Richtungswechsel.",
      "Slide all tiles in one direction. Equal neighbors merge into their sum; a new tile appears "
      "after a valid move. Reach 2048, then continue if you like. The game ends when no slide or "
      "merge is possible. Keep the largest tile in a corner with descending values nearby. Avoid "
      "direction changes that pull it out of position.",
      "Faites glisser les tuiles. Deux valeurs identiques fusionnent ; une nouvelle tuile apparaît "
      "après un coup valide. Atteignez 2048 puis continuez si vous voulez. La partie finit sans "
      "déplacement ou fusion possible. Gardez la plus grande valeur dans un coin, avec les valeurs "
      "décroissantes à côté."}},
    {64,
     {"Decke verdeckte Karten früh auf. Prüfe vor dem Ablegen, ob eine Karte noch als "
      "Zwischenablage gebraucht wird. Nicht jede Verteilung ist lösbar.",
      "Build four suit foundations from ace to king. Tableau columns descend and alternate colors; "
      "empty columns accept kings. Use the stock when no useful move remains. Prioritize revealing "
      "face-down cards. Before sending a card to a foundation, check whether it is still useful as "
      "a landing place in the tableau. Not every random deal is solvable.",
      "Construisez les fondations par couleur de l’as au roi. Les colonnes descendent et alternent "
      "les couleurs ; une colonne vide reçoit un roi. Utilisez la pioche au besoin. Découvrez les "
      "cartes cachées en priorité et vérifiez si une carte reste utile dans le tableau avant de "
      "l’envoyer à une fondation. Toutes les distributions ne sont pas solvables."}},
    {65,
     {"Suche einzelne Kandidaten und anschließend Zahlen, die in einer Zeile, Spalte oder einem "
      "Block nur noch an eine Stelle passen.",
      "Fill 1–9 once in every row, column and 3×3 box. Given numbers are fixed. Select a cell then "
      "a digit; pencil marks track possible candidates. Start with cells having only one "
      "candidate. Then look for a digit that has only one possible position in a row, column or "
      "box. Prefer logical elimination to guessing.",
      "Placez 1 à 9 une fois par ligne, colonne et bloc de 3×3. Les chiffres donnés sont fixes. "
      "Sélectionnez une case puis un chiffre ; les notes suivent les candidats. Cherchez une case "
      "à candidat unique, puis un chiffre qui ne peut occuper qu’une seule position dans une "
      "ligne, colonne ou boîte."}},
    {70,
     {"Buchstaben können mehrfach vorkommen. Eine einzelne passende Markierung bestätigt nicht "
      "automatisch mehrere Exemplare desselben Buchstabens.",
      "Guess the word in six attempts. Feedback distinguishes letters in the right position, "
      "letters elsewhere in the word, and absent letters. Start with common letters and use each "
      "guess to resolve uncertainty. A letter may repeat: one matching mark does not automatically "
      "confirm several copies.",
      "Devinez le mot en six essais. Les indications distinguent les lettres bien placées, "
      "présentes ailleurs et absentes. Commencez par des lettres fréquentes et utilisez chaque "
      "essai pour lever une incertitude. Une lettre peut se répéter : une indication positive ne "
      "confirme pas plusieurs exemplaires."}},
    {81,
     {"Beginne mit langen Gruppen, deren mögliche Positionen sich überlappen. Markiere sicher "
      "leere Zellen und übertrage Informationen zwischen Zeilen und Spalten.",
      "Edge clues describe consecutive runs of filled cells in order. Runs are separated by at "
      "least one empty cell. A clue 2, 1 means a pair followed later by one filled cell. Mark "
      "known empty cells. Start with long runs whose possible placements overlap, then transfer "
      "deductions between rows and columns.",
      "Les indices décrivent des groupes de cases pleines, dans l’ordre, séparés par au moins une "
      "case vide. L’indice 2, 1 signifie deux cases puis une case isolée. Marquez les cases "
      "certainement vides. Commencez par les longs groupes dont les placements se chevauchent, "
      "puis croisez lignes et colonnes."}},
    {82,
     {"Brücken bleiben stark, wenn zwei mögliche Verbindungsfelder offen sind. Achte auf "
      "gegnerische Unterbrechungen und sichere Anker am Rand.",
      "Connect your opposing edges with an unbroken chain of your stones. The opponent connects "
      "the other edges. Stones remain in place; there are no captures. Undo corrects accidental "
      "taps. Bridges are strong when either of two connecting cells remains available. Watch for "
      "opposing cuts and create firm edge anchors.",
      "Reliez vos bords opposés par une chaîne continue. L’adversaire relie l’autre paire de "
      "bords. Les pierres restent en place, sans capture. Annuler corrige les touchers "
      "accidentels. Les ponts sont solides si deux connexions restent possibles. Surveillez les "
      "coupures et créez des points d’ancrage au bord."}},
    {112,
     {"Die App stellt eine Karte bereit, zieht aber keine gemeinsamen Zahlen für eine Gruppe. "
      "Nutze dafür eine Person oder eine separate Ziehung.",
      "Five Bingo columns cover 1–15, 16–30, 31–45, 46–60 and 61–75. The center is free. Tap "
      "called numbers to mark them, tap again to unmark. Five marks in a row, column or diagonal "
      "make Bingo. This app provides your card, not a group caller. Use a person or separate "
      "number draw. Restart makes a fresh random card.",
      "Les cinq colonnes couvrent 1–15, 16–30, 31–45, 46–60 et 61–75. Le centre est libre. Touchez "
      "les nombres annoncés pour les marquer ou les retirer. Cinq marques alignées font Bingo. "
      "L’application fournit une carte, pas un tirage collectif. Utilisez une personne ou un "
      "tirage séparé."}},
    {113,
     {"Ein Gewinnfeld ist erst nutzbar, wenn seine unteren Felder gefüllt sind. Plane "
      "Doppel-Drohungen und blockiere gegnerische Dreierreihen.",
      "Two players drop stones in turn. Tap a column and the stone falls to its lowest empty cell. "
      "Four horizontally, vertically or diagonally win. A full board without a line is a draw. "
      "Plan from the bottom up: a winning cell needs support beneath it. Block immediate threats "
      "and try creating two winning moves at once.",
      "Deux joueurs lâchent une pierre à tour de rôle dans une colonne. Elle tombe sur la case "
      "libre la plus basse. Quatre pierres alignées gagnent ; un plateau plein sans alignement est "
      "nul. Prévoyez le support sous une case gagnante. Bloquez les menaces immédiates et cherchez "
      "deux gains simultanés."}},
    {117,
     {"Bevorzuge Ziele mit wenigen weiteren freien Anschlusszügen. Besonders Ecken und Ränder "
      "müssen früh eingeplant werden.",
      "Visit all 64 squares exactly once with a knight: two squares in one direction and one "
      "perpendicular. Dots show legal destinations; numbers show visiting order. Corners and edges "
      "have fewer exits. A useful heuristic chooses the target with the fewest onward moves. An "
      "early poor choice may become apparent much later.",
      "Visitez les 64 cases une fois avec un cavalier : deux cases puis une perpendiculaire. Les "
      "points indiquent les destinations légales et les nombres l’ordre de visite. Les coins et "
      "bords ont moins de sorties. Privilégiez une destination offrant peu de prolongements "
      "libres."}},
    {118,
     {"Die neuen Levels werden rückwärts aus einem gelösten Zustand erzeugt und sind daher lösbar. "
      "Nach dem Lösen startet das nächste Level automatisch. Brett und Kistenzahl wachsen. "
      "Vermeide Ecken ohne Ziel und halte die Wege zur benötigten Seite einer Kiste offen.",
      "Push every crate onto a goal. Push one crate at a time; pulling is impossible. Use the "
      "arrows or tap beside the player. Undo reverses the last step. Solving starts the next level "
      "automatically. Levels are generated backward from solved boards, guaranteeing a solution, "
      "and grow as you progress. Avoid corners without goals and keep routes open to the side of a "
      "crate you will need later.",
      "Poussez toutes les caisses sur les cibles, une seule à la fois. Tirer est impossible. "
      "Utilisez les flèches ou touchez près du joueur. Annuler revient d’un pas. Le niveau suivant "
      "démarre automatiquement. Les plateaux sont générés à rebours depuis une position résolue et "
      "grandissent progressivement. Évitez les coins sans cible et gardez des passages vers le "
      "côté nécessaire des caisses."}},
    {119,
     {"Die Pfeile bewegen einen Schritt. Tippe weiter in eine Richtung, um bis zum gewählten Ziel "
      "oder einer Wand zu laufen. Wischen bewegt mehrere Schritte. Merke dir Verzweigungen und "
      "kehre bei Sackgassen zur letzten offenen Abzweigung zurück.",
      "Reach the exit at the lower right. Restart generates a new connected maze. Arrows move one "
      "step; tapping farther in a direction walks toward that target until blocked. Swipes move "
      "several steps. Remember junctions rather than individual cells. At a dead end, return to "
      "the last unexplored branch. The exit is reachable and there is no time limit.",
      "Atteignez la sortie en bas à droite. Redémarrer crée un nouveau labyrinthe connecté. Les "
      "flèches avancent d’un pas ; toucher plus loin marche vers la cible jusqu’à un obstacle. Un "
      "glissement avance de plusieurs pas. Mémorisez les intersections et revenez à la dernière "
      "branche inexplorée après une impasse. La sortie est accessible, sans limite de temps."}},
    {120,
     {"Nummern und Richtungspfeile ordnen die Hinweise dem Raster zu. Tippe einen Hinweis oder "
      "eine Zelle an. Gib das vollständige englische Wort ein. Wortlänge und Kreuzungen werden "
      "geprüft. Mit dem Häkchen prüfst du die Lösungen, mit Neustart erzeugst du ein neues Rätsel.",
      "Numbers and arrows identify across and down words. Tap a clue or grid cell and enter the "
      "complete English answer. Crossings must share the same letter. The app checks length and "
      "conflicting crossings immediately. The checkmark compares every answer with the solution; "
      "restart generates another puzzle. Begin with a short answer you know, then use its crossing "
      "letters as anchors.",
      "Les numéros et flèches indiquent les mots horizontaux et verticaux. Touchez un indice ou "
      "une case et saisissez la réponse complète en anglais. Les croisements partagent une lettre. "
      "La longueur et les contradictions sont vérifiées. La coche compare les réponses aux "
      "solutions ; redémarrer crée un autre jeu. Commencez par une réponse certaine."}},
    {121,
     {"Du musst nicht jedes Monster bekämpfen. Plane einen günstigen Weg zur Treppe. Jede neue "
      "Ebene gibt drei Lebenspunkte, ein besiegtes Monster kostet zwei.",
      "Collect coins, defeat monsters and find the stairs. Crosses are monsters, rings coins and "
      "steps the staircase. Move with the arrows. Defeating a monster costs two health; descending "
      "restores three health and creates new loot. At zero health the journey ends. You need not "
      "fight every enemy: plan an efficient route, and use Undo after mistakes. This is a small "
      "turn-based exploration game.",
      "Ramassez les pièces, vainquez les monstres et trouvez l’escalier. Les croix sont les "
      "monstres, les anneaux les pièces. Utilisez les flèches. Un combat coûte deux points de vie "
      "; descendre en rend trois et renouvelle le butin. À zéro l’aventure finit. Évitez les "
      "combats inutiles et annulez les erreurs. Le jeu est au tour par tour."}},
};
static void u17_game_manual(int route, const char *title, const char *fallback) {
    const char *text = fallback;
    for (unsigned i = 0; i < sizeof(manuals17) / sizeof(*manuals17); i++)
        if (manuals17[i].route == route)
            text = manuals17[i].text[options[OPT_LANGUAGE] >= 0 && options[OPT_LANGUAGE] < 3
                                         ? options[OPT_LANGUAGE]
                                         : 0];
    char *body = NULL;
    size_t n = 0;
    FILE *f = open_memstream(&body, &n);
    if (!f)
        return;
    fprintf(f, "# %s\n\n", tr(title));
    if (!options[OPT_LANGUAGE])
        fprintf(f, "%s\n\n## Strategie und Bedienung\n\n", fallback);
    fprintf(f, "%s", text);
    fclose(f);
    reader_markdown17(tr(title), body, route);
    free(body);
}
