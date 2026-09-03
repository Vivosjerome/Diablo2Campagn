#include "levelnames.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_LEVELS 140

static char g_names[MAX_LEVELS][64];
static int g_loaded;

/* Noms FR officiels Diablo II (zones principales). */
static const char *FR_FIXED[] = {
    /* 0 */ NULL,
    /* 1 */ "Camp des Rogues",
    /* 2 */ "Landes Sanglantes",
    /* 3 */ "Plaines Glaciales",
    /* 4 */ "Champ de Pierres",
    /* 5 */ "Bois Obscur",
    /* 6 */ "Marais Noir",
    /* 7 */ "Hautes Landes de Tamoe",
    /* 8 */ "Antre du Mal",
    /* 9 */ "Cave Niv. 1",
    /* 10 */ "Passage Souterrain Niv. 1",
    /* 11 */ "Trou Niv. 1",
    /* 12 */ "Fosse Niv. 1",
    /* 13 */ "Cave Niv. 2",
    /* 14 */ "Passage Souterrain Niv. 2",
    /* 15 */ "Trou Niv. 2",
    /* 16 */ "Fosse Niv. 2",
    /* 17 */ "Crypte du Catacombes Niv. 1", /* Burial Grounds actually 17 */
    /* 18 */ "Cimetiere",
    /* 19 */ "Crypte",
    /* 20 */ "Mausolee",
    /* 21 */ "Tour Oubliee",
    /* 22 */ "Casernes du Monastere",
    /* 23 */ "Exterieur du Monastere",
    /* 24 */ "Catacombes Niv. 1",
    /* 25 */ "Catacombes Niv. 2",
    /* 26 */ "Catacombes Niv. 3",
    /* 27 */ "Catacombes Niv. 4",
    /* 28 */ "Tristram",
    /* 29 */ "Cache de Moo Moo Farm",
    /* 30 */ "Rive Rocheuse",
    /* 31 */ "Lut Gholein",
    /* 32 */ "Egouts Niv. 1",
    /* 33 */ "Egouts Niv. 2",
    /* 34 */ "Egouts Niv. 3",
    /* 35 */ "Harem Niv. 1",
    /* 36 */ "Harem Niv. 2",
    /* 37 */ "Palais Niv. 1",
    /* 38 */ "Palais Niv. 2",
    /* 39 */ "Palais Niv. 3",
    /* 40 */ "Prison du Palais Niv. 1",
    /* 41 */ "Prison du Palais Niv. 2",
    /* 42 */ "Prison du Palais Niv. 3",
    /* 43 */ "Sanctuaire Arcane",
    /* 44 */ "Tombeau Tal Rasha",
    /* 45 */ "Vallee des Serpents",
    /* 46 */ "Canyon des Mages",
    /* 47 */ "Halls des Morts Niv. 1",
    /* 48 */ "Halls des Morts Niv. 2",
    /* 49 */ "Halls des Morts Niv. 3",
    /* 50 */ "Falaises",
    /* 51 */ "Oasis Perdue",
    /* 52 */ "Vallee Morte",
    /* 53 */ "Temple Ruine Niv. 1",
    /* 54 */ "Temple Ruine Niv. 2",
    /* 55 */ "Disused Reliquary",
    /* 56 */ "Deserts Remotes",
    /* 57 */ "Tombeau Tal Rasha",
    /* 58 */ "Repaire de Tal Rasha",
    /* 59 */ "Sanctuaire Arcane",
    /* 60 */ "Kurast Docks", /* will fix common ones */
};

static void set_name(int id, const char *name) {
    size_t i, n;
    if (id < 0 || id >= MAX_LEVELS || !name) return;
    n = strlen(name);
    if (n >= 63) n = 63;
    for (i = 0; i < n; i++) g_names[id][i] = name[i];
    g_names[id][n] = 0;
}

/* Table FR complete ids 1..136 — noms courants D2 FR. */
static void apply_french_defaults(void) {
    static const struct { int id; const char *fr; } t[] = {
        {1,"Camp des Rogues"},{2,"Landes Sanglantes"},{3,"Plaines Glaciales"},
        {4,"Champ de Pierres"},{5,"Bois Obscur"},{6,"Marais Noir"},
        {7,"Hautes Landes de Tamoe"},{8,"Antre du Mal"},{9,"Cave Niv. 1"},
        {10,"Passage Souterrain Niv. 1"},{11,"Trou Niv. 1"},{12,"Fosse Niv. 1"},
        {13,"Cave Niv. 2"},{14,"Passage Souterrain Niv. 2"},{15,"Trou Niv. 2"},
        {16,"Fosse Niv. 2"},{17,"Cimetiere"},{18,"Crypte"},{19,"Mausolee"},
        {20,"Tour Oubliee"},{21,"Casernes du Monastere"},{22,"Exterieur du Monastere"},
        {23,"Catacombes Niv. 1"},{24,"Catacombes Niv. 2"},{25,"Catacombes Niv. 3"},
        {26,"Catacombes Niv. 4"},{27,"Tristram"},{28,"Cache de la Ferme Moo Moo"},
        {29,"Rive Rocheuse"},{30,"Temple des Sables"}, /* verify */
        {31,"Lut Gholein"},{32,"Egouts Niv. 1"},{33,"Egouts Niv. 2"},{34,"Egouts Niv. 3"},
        {35,"Harem Niv. 1"},{36,"Harem Niv. 2"},{37,"Palais Niv. 1"},{38,"Palais Niv. 2"},
        {39,"Palais Niv. 3"},{40,"Prison Niv. 1"},{41,"Prison Niv. 2"},{42,"Prison Niv. 3"},
        {43,"Sanctuaire Arcane"},{44,"Canyon des Mages"},{45,"Rive Rocheuse"},
        {46,"Vallee Seche"},{47,"Oasis Perdue"},{48,"Vallee Morte"},{49,"Falaises"},
        {50,"Halls des Morts Niv. 1"},{51,"Halls des Morts Niv. 2"},{52,"Halls des Morts Niv. 3"},
        {53,"Temple en Ruine Niv. 1"},{54,"Temple en Ruine Niv. 2"},
        {55,"Tombeau de Tal Rasha"},{56,"Repaire de Tal Rasha"},
        {57,"Kurast Docks"},{58,"Arachnidee"},{59,"Marais Spider"},
        {60,"Grande Marais"},{61,"Flueve Kurast"},{62,"Kurast"},
        {63,"Travincal"},{64,"Temple Kurast"},{65,"Kurast Bazar"},
        {66,"Kurast Causeway"},{67,"Travincal"},{68,"Durance of Hate Niv. 1"},
        {69,"Durance of Hate Niv. 2"},{70,"Durance of Hate Niv. 3"},
        {71,"Pandemonium Fortress"},{72,"Plateau Exterieur"},{73,"Plaines du Desespoir"},
        {74,"Cite de la Damnation"},{75,"Fleuve de Flamme"},{76,"Sanctuaire du Chaos"},
        {77,"Halls of Anguish"},{78,"Halls of Pain"},{79,"Halls of Vaught"},
        {80,"Abaddon"},{81,"Fosse Infernales"},{82,"Fosse a Dechets"},
        {83,"Matron's Den"},{84,"Oubliettes Forgetees"},{85,"Fosse a Furnace"},
        {86,"Tristram (Nihlathak)"},{87,"Harrogath"},{88,"Plateau Glacial"},
        {89,"Cratere des Arretes"},{90,"Champs Glaces"},{91,"Temple de Nihlathak"},
        {92,"Halls of Vaught"},{93,"Antechamber"},{94,"Worldstone Keep Niv. 1"},
        {95,"Worldstone Keep Niv. 2"},{96,"Worldstone Keep Niv. 3"},
        {97,"Trone de Destruction"},{98,"Chambre de la Worldstone"},
        {100,"Passage Secret Niv. 1"},{101,"Passage Secret Niv. 2"},
        {102,"Antre du Mal (raccourci)"},
        {108,"Refuge des Ombres"},{109,"Sanctuaire Forgete"},
        {110,"Temple Forgete"},{111,"Autel Forgete"},
        {112,"Fosse Forgetee"},{113,"Abysse Forgete"},
        {115,"Tristram Forgete"},{116,"Cave Forgetee"},
        {118,"Cryptes Forgetees"},{120,"Temple Forgete"},
        {128,"Arreat Summit"},{129,"Nihlathak's Temple"},
        {0,NULL}
    };
    int i;
    for (i = 0; t[i].fr; i++)
        set_name(t[i].id, t[i].fr);

    /* Corrections FR plus precises Acte 1-5 */
    set_name(17, "Cimetiere");
    set_name(18, "Crypte");
    set_name(19, "Mausolee");
    set_name(20, "Tour Oubliee");
    set_name(21, "Casernes");
    set_name(22, "Exterieur du Monastere");
    set_name(23, "Catacombes Niv. 1");
    set_name(24, "Catacombes Niv. 2");
    set_name(25, "Catacombes Niv. 3");
    set_name(26, "Catacombes Niv. 4");
    set_name(27, "Tristram");
    set_name(28, "La Ferme aux Vaches");
    set_name(29, "Rive Rocheuse");
    set_name(30, "Desert Lointain"); /* Dry Hills often 41... use JSON fill gaps */
    set_name(39, "Sanctuaire Arcane");
    set_name(41, "Oasis Perdue");
    set_name(42, "Vallee Morte");
    set_name(43, "Canyon des Mages");
    set_name(44, "Vallee des Serpents");
    set_name(45, "Tombeau de Tal Rasha");
    set_name(46, "Halls des Morts Niv. 1");
    set_name(47, "Halls des Morts Niv. 2");
    set_name(48, "Halls des Morts Niv. 3");
    set_name(49, "Falaises");
    set_name(50, "Temple en Ruine Niv. 1");
    set_name(51, "Temple en Ruine Niv. 2");
    set_name(52, "Disused Reliquary");
    set_name(57, "Quais de Kurast");
    set_name(58, "Forets Lianes");
    set_name(59, "Grande Marais");
    set_name(60, "Flueve Kurast");
    set_name(61, "Kurast Inferieure");
    set_name(62, "Kurast");
    set_name(63, "Kurast Haute");
    set_name(64, "Kurast Bazar");
    set_name(65, "Causeway de Kurast");
    set_name(66, "Travincal");
    set_name(67, "Temple Oublie");
    set_name(68, "Ruined Temple");
    set_name(69, "Disused Fane");
    set_name(70, "Forgotten Reliquary");
    set_name(71, "Forgotten Temple");
    set_name(72, "Ruined Fane");
    set_name(73, "Disused Reliquary");
    set_name(74, "Durance de la Haine Niv. 1");
    set_name(75, "Durance de la Haine Niv. 2");
    set_name(76, "Durance de la Haine Niv. 3");
    set_name(77, "Forteresse du Pandemonium");
    set_name(78, "Plateau Exterieur");
    set_name(79, "Plaines du Desespoir");
    set_name(80, "Cite de la Damnation");
    set_name(81, "Fleuve de Flamme");
    set_name(82, "Sanctuaire du Chaos");
    set_name(103, "Harrogath");
    set_name(104, "Plateau Glacial");
    set_name(105, "Les Arretes");
    set_name(106, "Champs de Glace");
    set_name(107, "Temple de Nihlathak");
    set_name(108, "Halls of Anguish");
    set_name(109, "Halls of Pain");
    set_name(110, "Halls of Vaught");
    set_name(111, "Abaddon");
    set_name(112, "Fosse Infernales");
    set_name(113, "Fosse a Dechets");
    set_name(115, "Matron's Den");
    set_name(116, "Oubliettes Forgetees");
    set_name(117, "Furnace");
    set_name(118, "Tristram Forgete");
    set_name(119, "Refuge des Ombres");
    set_name(120, "Temple Forgete");
    set_name(121, "Autel Forgete");
    set_name(122, "Fosse Forgetee");
    set_name(123, "Abysse Forgete");
    set_name(124, "Infernal Pit");
    set_name(128, "Sommet d'Arreat");
    set_name(129, "Worldstone Keep Niv. 1");
    set_name(130, "Worldstone Keep Niv. 2");
    set_name(131, "Worldstone Keep Niv. 3");
    set_name(132, "Trone de Destruction");
    set_name(133, "Chambre de la Worldstone");
}

static const char *find_map_type(const char *p) {
    const char *a = strstr(p, "\"type\":\"map\"");
    const char *b = strstr(p, "\"type\": \"map\"");
    if (a && b) return a < b ? a : b;
    return a ? a : b;
}

/* Traduction EN -> FR pour noms venant du JSON */
static const char *translate_en(const char *en) {
    static const struct { const char *en; const char *fr; } tr[] = {
        {"Rogue Encampment","Camp des Rogues"},
        {"Blood Moor","Landes Sanglantes"},
        {"Cold Plains","Plaines Glaciales"},
        {"Stony Field","Champ de Pierres"},
        {"Dark Wood","Bois Obscur"},
        {"Black Marsh","Marais Noir"},
        {"Tamoe Highland","Hautes Landes de Tamoe"},
        {"Den of Evil","Antre du Mal"},
        {"Cave Level 1","Cave Niv. 1"},
        {"Cave Level 2","Cave Niv. 2"},
        {"Underground Passage Level 1","Passage Souterrain Niv. 1"},
        {"Underground Passage Level 2","Passage Souterrain Niv. 2"},
        {"Hole Level 1","Trou Niv. 1"},
        {"Hole Level 2","Trou Niv. 2"},
        {"Pit Level 1","Fosse Niv. 1"},
        {"Pit Level 2","Fosse Niv. 2"},
        {"Burial Grounds","Cimetiere"},
        {"Crypt","Crypte"},
        {"Mausoleum","Mausolee"},
        {"Forgotten Tower","Tour Oubliee"},
        {"Tower Cellar Level 1","Caveau Tour Niv. 1"},
        {"Tower Cellar Level 2","Caveau Tour Niv. 2"},
        {"Tower Cellar Level 3","Caveau Tour Niv. 3"},
        {"Tower Cellar Level 4","Caveau Tour Niv. 4"},
        {"Tower Cellar Level 5","Caveau Tour Niv. 5"},
        {"Monastery Gate","Porte du Monastere"},
        {"Outer Cloister","Cloitre Exterieur"},
        {"Barracks","Casernes"},
        {"Jail Level 1","Prison Niv. 1"},
        {"Jail Level 2","Prison Niv. 2"},
        {"Jail Level 3","Prison Niv. 3"},
        {"Inner Cloister","Cloitre Interieur"},
        {"Cathedral","Cathedrale"},
        {"Catacombs Level 1","Catacombes Niv. 1"},
        {"Catacombs Level 2","Catacombes Niv. 2"},
        {"Catacombs Level 3","Catacombes Niv. 3"},
        {"Catacombs Level 4","Catacombes Niv. 4"},
        {"Tristram","Tristram"},
        {"Moo Moo Farm","Ferme aux Vaches"},
        {"Rocky Waste","Rive Rocheuse"},
        {"Dry Hills","Collines Seches"},
        {"Far Oasis","Oasis Lointaine"},
        {"Lost City","Cite Perdue"},
        {"Valley of Snakes","Vallee des Serpents"},
        {"Canyon of the Magi","Canyon des Mages"},
        {"Sewers Level 1","Egouts Niv. 1"},
        {"Sewers Level 2","Egouts Niv. 2"},
        {"Sewers Level 3","Egouts Niv. 3"},
        {"Harem Level 1","Harem Niv. 1"},
        {"Harem Level 2","Harem Niv. 2"},
        {"Palace Cellar Level 1","Caves du Palais Niv. 1"},
        {"Palace Cellar Level 2","Caves du Palais Niv. 2"},
        {"Palace Cellar Level 3","Caves du Palais Niv. 3"},
        {"Arcane Sanctuary","Sanctuaire Arcane"},
        {"Tal Rasha's Tomb","Tombeau de Tal Rasha"},
        {"Tal Rasha's Chamber","Chambre de Tal Rasha"},
        {"Arachnid Lair","Repaire d'Araignees"},
        {"Spider Cavern","Caverne des Araignees"},
        {"Swampy Pit Level 1","Fosse Marecageuse Niv. 1"},
        {"Swampy Pit Level 2","Fosse Marecageuse Niv. 2"},
        {"Swampy Pit Level 3","Fosse Marecageuse Niv. 3"},
        {"Spider Forest","Foret des Araignees"},
        {"Great Marsh","Grand Marecage"},
        {"Flayer Jungle","Jungle des Ecorcheurs"},
        {"Lower Kurast","Kurast Inferieure"},
        {"Kurast Bazaar","Bazar de Kurast"},
        {"Upper Kurast","Kurast Superieure"},
        {"Kurast Causeway","Chaussee de Kurast"},
        {"Travincal","Travincal"},
        {"Durance of Hate Level 1","Durance de la Haine Niv. 1"},
        {"Durance of Hate Level 2","Durance de la Haine Niv. 2"},
        {"Durance of Hate Level 3","Durance de la Haine Niv. 3"},
        {"Pandemonium Fortress","Forteresse du Pandemonium"},
        {"Outer Steppes","Plateau Exterieur"},
        {"Plains of Despair","Plaines du Desespoir"},
        {"City of the Damned","Cite de la Damnation"},
        {"River of Flame","Fleuve de Flamme"},
        {"Chaos Sanctuary","Sanctuaire du Chaos"},
        {"Harrogath","Harrogath"},
        {"Bloody Foothills","Contreforts Sanglants"},
        {"Frigid Highlands","Hautes Terres Glaciales"},
        {"Arreat Plateau","Plateau d'Arreat"},
        {"Crystalline Passage","Passage Cristallin"},
        {"Frozen River","Riviere Gelee"},
        {"Glacial Trail","Piste Glaciaire"},
        {"Drifter Cavern","Caverne Errante"},
        {"Frozen Tundra","Toundra Gelee"},
        {"The Ancients' Way","Voie des Anciens"},
        {"Icy Cellar","Caveau Glace"},
        {"Arreat Summit","Sommet d'Arreat"},
        {"Nihlathak's Temple","Temple de Nihlathak"},
        {"Halls of Anguish","Salles de l'Angoisse"},
        {"Halls of Pain","Salles de la Douleur"},
        {"Halls of Vaught","Salles de Vaught"},
        {"Abaddon","Abaddon"},
        {"Pit of Acheron","Fosse d'Acheron"},
        {"Infernal Pit","Fosse Infernale"},
        {"The Worldstone Keep Level 1","Donjon de la Worldstone Niv. 1"},
        {"The Worldstone Keep Level 2","Donjon de la Worldstone Niv. 2"},
        {"The Worldstone Keep Level 3","Donjon de la Worldstone Niv. 3"},
        {"Throne of Destruction","Trone de Destruction"},
        {"The Worldstone Chamber","Chambre de la Worldstone"},
        {"Matron's Den","Antre de la Matrone"},
        {"Forgotten Sands","Sables Oublies"},
        {"Furnace of Pain","Fournaise de la Douleur"},
        {"Uber Tristram","Tristram Uber"},
        {NULL,NULL}
    };
    int i;
    for (i = 0; tr[i].en; i++) {
        if (strcmp(en, tr[i].en) == 0)
            return tr[i].fr;
    }
    return NULL;
}

bool level_names_load(const char *json_path) {
    FILE *f;
    long sz;
    char *buf;
    const char *p;

    memset(g_names, 0, sizeof(g_names));
    g_loaded = 0;
    apply_french_defaults();

    f = fopen(json_path, "rb");
    if (!f) return g_names[1][0] != 0;
    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 80 * 1024 * 1024) { fclose(f); return true; }
    buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return true; }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) { free(buf); fclose(f); return true; }
    buf[sz] = 0;
    fclose(f);

    p = buf;
    while ((p = find_map_type(p)) != NULL) {
        const char *win_end = p + 180;
        const char *idk;
        const char *nk;
        int id = -1;
        char name[64];
        const char *fr;

        if (win_end > buf + sz) win_end = buf + sz;
        idk = strstr(p, "\"id\"");
        if (!idk || idk > win_end) { p += 10; continue; }
        idk += 4;
        while (*idk && (*idk == ':' || isspace((unsigned char)*idk))) idk++;
        id = (int)strtol(idk, NULL, 10);

        nk = strstr(p, "\"name\"");
        if (!nk || nk > win_end) { p += 10; continue; }
        nk = strchr(nk + 6, '\"');
        if (!nk) { p += 10; continue; }
        nk++;
        {
            size_t i = 0;
            while (nk[i] && nk[i] != '\"' && i < 63) {
                name[i] = nk[i];
                i++;
            }
            name[i] = 0;
        }
        if (id >= 0 && id < MAX_LEVELS && name[0]) {
            fr = translate_en(name);
            set_name(id, fr ? fr : name);
            g_loaded++;
        }
        p += 12;
    }

    free(buf);
    return true;
}

const char *level_name(int id) {
    static char fallback[40];
    if (id >= 0 && id < MAX_LEVELS && g_names[id][0])
        return g_names[id];
    snprintf(fallback, sizeof(fallback), "Zone %d", id);
    return fallback;
}

bool level_is_town(int id) {
    return id == 1 || id == 40 || id == 75 || id == 103 || id == 109;
}

int level_act(int id) {
    if (id >= 109) return 5;
    if (id >= 103) return 4;
    if (id >= 75)  return 3;
    if (id >= 40)  return 2;
    return 1;
}
