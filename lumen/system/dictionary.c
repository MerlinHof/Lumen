#include "dictionary.h"
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include "library.h"
#include <string.h>
typedef struct { const char *word, *definition; } Entry;
static const Entry entries[][3] = {
    {{"Buch", "Gebundene oder digitale Sammlung von Seiten."}, {"Book", "A written work made up of pages."}, {"Livre", "Ouvrage écrit composé de pages."}},
    {{"Baum", "Holzpflanze mit Stamm und Krone."}, {"Tree", "A tall plant with a woody trunk."}, {"Arbre", "Plante ligneuse formée d'un tronc et d'une couronne."}},
    {{"Wald", "Fläche, die dicht mit Bäumen bewachsen ist."}, {"Forest", "A large area covered with trees."}, {"Forêt", "Vaste étendue couverte d'arbres."}},
    {{"Insel", "Land, das auf allen Seiten von Wasser umgeben ist."}, {"Island", "Land completely surrounded by water."}, {"Île", "Terre entourée d'eau de tous côtés."}},
    {{"Fluss", "Natürliches Gewässer mit dauerhafter Strömung."}, {"River", "A natural stream of flowing water."}, {"Rivière", "Cours d'eau naturel qui s'écoule dans un autre cours d'eau."}},
    {{"Ozean", "Eines der großen zusammenhängenden Weltmeere."}, {"Ocean", "One of the large connected bodies of salt water."}, {"Océan", "Vaste étendue continue d'eau salée."}},
    {{"Wolke", "Ansammlung kleinster Wassertröpfchen oder Eiskristalle in der Luft."}, {"Cloud", "A visible mass of water droplets or ice crystals in the air."}, {"Nuage", "Masse visible de gouttelettes d'eau ou de cristaux de glace."}},
    {{"Stern", "Himmelskörper, der aus eigener Kraft leuchtet."}, {"Star", "A luminous celestial body that produces its own light."}, {"Étoile", "Astre qui émet sa propre lumière."}},
    {{"Mond", "Natürlicher Begleiter der Erde."}, {"Moon", "Earth's natural satellite."}, {"Lune", "Satellite naturel de la Terre."}},
    {{"Sonne", "Der Stern im Zentrum unseres Planetensystems."}, {"Sun", "The star at the center of our solar system."}, {"Soleil", "Étoile située au centre du Système solaire."}},
    {{"Licht", "Strahlung, die das menschliche Auge wahrnehmen kann."}, {"Light", "Radiation visible to the human eye."}, {"Lumière", "Rayonnement visible par l'œil humain."}},
    {{"Stein", "Festes Stück Gestein."}, {"Stone", "A solid piece of rock."}, {"Pierre", "Morceau de roche solide."}},
    {{"Zeit", "Maß für die Dauer und Reihenfolge von Ereignissen."}, {"Time", "A measure of duration and sequence."}, {"Temps", "Mesure de la durée et de la succession des événements."}},
    {{"Papier", "Dünnes Material aus gepressten Pflanzenfasern."}, {"Paper", "Thin material made from pressed plant fibers."}, {"Papier", "Matière mince fabriquée à partir de fibres végétales pressées."}},
    {{"Tinte", "Flüssigkeit zum Schreiben, Zeichnen oder Drucken."}, {"Ink", "A liquid used for writing, drawing or printing."}, {"Encre", "Liquide utilisé pour écrire, dessiner ou imprimer."}},
    {{"Reise", "Weg zu einem weiter entfernten Ort."}, {"Journey", "Travel from one place to another."}, {"Voyage", "Déplacement vers un lieu éloigné."}},
    {{"Nacht", "Zeit zwischen Sonnenuntergang und Sonnenaufgang."}, {"Night", "The period between sunset and sunrise."}, {"Nuit", "Période comprise entre le coucher et le lever du soleil."}},
    {{"Garten", "Fläche, auf der Pflanzen gepflegt werden."}, {"Garden", "A place where plants are grown and tended."}, {"Jardin", "Terrain où l'on cultive et entretient des plantes."}},
    {{"Musik", "Kunst, Klänge und Stille in der Zeit zu gestalten."}, {"Music", "The art of arranging sounds and silence in time."}, {"Musique", "Art d'organiser des sons et des silences dans le temps."}},
    {{"Brücke", "Bauwerk, das einen Weg über ein Hindernis führt."}, {"Bridge", "A structure that carries a route over an obstacle."}, {"Pont", "Ouvrage qui permet de franchir un obstacle."}},
    {{"Traum", "Bilder und Erlebnisse während des Schlafs."}, {"Dream", "Images or experiences that occur during sleep."}, {"Rêve", "Images ou expériences qui surviennent pendant le sommeil."}},
    {{"Frosch", "Amphibium mit langen Hinterbeinen zum Springen."}, {"Frog", "An amphibian with long hind legs adapted for jumping."}, {"Grenouille", "Amphibien aux longues pattes arrière adaptées au saut."}},
    {{"Uhr", "Gerät zum Messen und Anzeigen der Zeit."}, {"Clock", "A device that measures and displays time."}, {"Horloge", "Appareil servant à mesurer et à afficher l'heure."}},
    {{"Seite", "Eine Fläche eines Blatts oder eine Bildschirmansicht."}, {"Page", "One side of a sheet, or a single screen of content."}, {"Page", "Une face d'une feuille ou un écran de contenu."}},
};

static void word_key(const char *input, char out[64]) {
    size_t n = 0;
    while (*input && n < 63) {
        unsigned char c = *input++;
        if (c == 0xc3 && *input) {
            unsigned char accent = (unsigned char)*input++ | 0x20;
            if (accent == 0xbf && n < 62) { out[n++] = 'S'; c = 'S'; }
            else if (accent >= 0xa0 && accent <= 0xa5) c = 'A';
            else if (accent == 0xa7) c = 'C';
            else if (accent >= 0xa8 && accent <= 0xab) c = 'E';
            else if (accent >= 0xac && accent <= 0xaf) c = 'I';
            else if (accent == 0xb1) c = 'N';
            else if (accent >= 0xb2 && accent <= 0xb6) c = 'O';
            else if (accent >= 0xb9 && accent <= 0xbc) c = 'U';
            else c = 0;
        }
        if (c < 128 && isalpha(c)) out[n++] = (char)toupper(c);
        else if (n && c <= 32) break;
    }
    out[n] = 0;
}
const char *dictionary_lookup(const char *word, int preferred, int *language) {
    char key[64], candidate[64];
    word_key(word, key);
    if (!*key) return NULL;
    if (preferred < 0 || preferred > 2) preferred = 0;
    for (int offset = 0; offset < 3; offset++) {
        int lang = (preferred + offset) % 3;
        for (size_t i = 0; i < sizeof(entries)/sizeof(*entries); i++) {
            word_key(entries[i][lang].word, candidate);
            if (!strcmp(key, candidate)) {
                if (language) *language = lang;
                return entries[i][lang].definition;
            }
        }
    }
    return NULL;
}
static int read_offset(FILE *file, uint32_t *value) {
    unsigned char bytes[4];
    if (fread(bytes, 1, sizeof(bytes), file) != sizeof(bytes)) return 0;
    *value = bytes[0] | (uint32_t)bytes[1] << 8 |
             (uint32_t)bytes[2] << 16 | (uint32_t)bytes[3] << 24;
    return 1;
}
int dictionary_translate(const char *word, int preferred, char *result, size_t size) {
    static const char *pairs[] = {"deu-fra", "eng-fra", "fra-eng"};
    char key[64];
    word_key(word, key);
    if (!*key || !size) return 0;
    if (preferred < 0 || preferred > 2) preferred = 0;
    for (int attempt = 0; attempt < 3; attempt++) {
        const char *pair = pairs[(preferred + attempt) % 3];
        char path[1536];
#ifdef LUMEN_HOST
        snprintf(path, sizeof(path), "lumen/assets/dictionaries/%s.idx", pair);
#else
        snprintf(path, sizeof(path), "%s/../dictionary/%s.idx", data_root, pair);
#endif
        FILE *file = fopen(path, "rb");
        if (!file) continue;
        uint32_t count;
        if (!read_offset(file, &count) || count > 1000000) {
            fclose(file);
            continue;
        }
        uint32_t first = 0, last = count;
        while (first < last) {
            uint32_t middle = first + (last - first) / 2, offset;
            char line[450];
            if (fseek(file, 4 + 4 * (long)middle, SEEK_SET) ||
                !read_offset(file, &offset) || offset < 4 + 4 * count ||
                fseek(file, offset, SEEK_SET) || !fgets(line, sizeof(line), file)) break;
            char *separator = strchr(line, '\t');
            if (!separator) break;
            *separator = 0;
            int compare = strcmp(key, line);
            if (!compare) {
                separator++;
                separator[strcspn(separator, "\r\n")] = 0;
                snprintf(result, size, "%s · %s", pair, separator);
                fclose(file);
                return 1;
            }
            if (compare > 0) first = middle + 1;
            else last = middle;
        }
        fclose(file);
    }
    return 0;
}
void dictionary_game_word(int language, unsigned random, char out[64]) {
    if (language < 0 || language > 2) language = 0;
    word_key(entries[random % (sizeof(entries)/sizeof(*entries))][language].word, out);
}
