#include "bossstate.h"
#include <string.h>

typedef struct {
    uint32_t txt;
    int level;
    int always_present;
    int ignore_unloaded; /* 1 = absent de la memoire = trop loin, pas mort */
} BossTxtTrack;

typedef struct {
    int level;
    const char *quest_name;
    uint32_t boss_txt;
} QuestBossLink;

static const BossTxtTrack BOSS_TXT_TRACK[] = {
    { 229,  49, 1, 0 }, /* Radament */
    { 250,  74, 1, 0 }, /* Summoner */
    { 256, 105, 0, 1 }, /* Izual */
    { 365,  33, 1, 0 }, /* Griswold */
    { 409, 107, 0, 0 }, /* Hephasto (forge) */
    { 526, 124, 0, 0 }, /* Nihlathak */
    {   0,   0, 0, 0 }
};

static const QuestBossLink QUEST_BOSS_LINK[] = {
    { 107, "Forge infernale", 409 },
    { 107, "Hellforge", 409 },
    {   0, NULL, 0 }
};

#define TXT_SLOTS 1024

static int g_txt_seen[TXT_SLOTS];
static int g_txt_dead[TXT_SLOTS];
static int g_txt_absent[TXT_SLOTS];
static int g_last_level;
static uint32_t g_seed;

static int str_ieq(const char *a, const char *b) {
    if (!a || !b) return 0;
    while (*a && *b) {
        int ca = (*a >= 'A' && *a <= 'Z') ? *a + 32 : *a;
        int cb = (*b >= 'A' && *b <= 'Z') ? *b + 32 : *b;
        if (ca != cb) return 0;
        a++;
        b++;
    }
    return *a == *b;
}

static int monsters_has_txt(const MonList *monsters, uint32_t txt) {
    int i;
    if (!monsters || txt == 0) return 0;
    for (i = 0; i < monsters->count; i++)
        if (monsters->list[i].txt == txt)
            return 1;
    return 0;
}

static const BossTxtTrack *track_for_level(int level) {
    int i;
    for (i = 0; BOSS_TXT_TRACK[i].txt; i++)
        if (BOSS_TXT_TRACK[i].level == level)
            return &BOSS_TXT_TRACK[i];
    return NULL;
}

static uint32_t txt_for_level(int level) {
    const BossTxtTrack *tr = track_for_level(level);
    return tr ? tr->txt : 0;
}

void boss_state_reset(uint32_t seed) {
    if (g_seed == seed && g_seed != 0) return;
    memset(g_txt_seen, 0, sizeof(g_txt_seen));
    memset(g_txt_dead, 0, sizeof(g_txt_dead));
    memset(g_txt_absent, 0, sizeof(g_txt_absent));
    g_last_level = 0;
    g_seed = seed;
}

void boss_state_update(uint32_t level_id, const MonList *monsters, const D2Process *p) {
    int i;
    uint32_t txt;

    if ((int)level_id != g_last_level) {
        g_last_level = (int)level_id;
        memset(g_txt_absent, 0, sizeof(g_txt_absent));
    }

    for (i = 0; BOSS_TXT_TRACK[i].txt; i++) {
        const BossTxtTrack *tr = &BOSS_TXT_TRACK[i];
        int probe = 0;
        int on_level;
        int alive;

        txt = tr->txt;
        if (txt >= TXT_SLOTS) continue;
        on_level = ((int)level_id == tr->level);
        alive = monsters_has_txt(monsters, txt);
        if (p && on_level)
            probe = boss_probe_on_level(p, (uint32_t)tr->level, txt);

        if (probe == 1 || alive) {
            g_txt_seen[txt] = 1;
            g_txt_absent[txt] = 0;
            continue;
        }

        if (probe == 2 && on_level) {
            g_txt_dead[txt] = 1;
            continue;
        }

        if (!on_level) continue;

        /* Grande map : pas en memoire = trop loin, le chemin doit rester. */
        if (tr->ignore_unloaded && probe == 0)
            continue;

        if (g_txt_seen[txt]) {
            g_txt_dead[txt] = 1;
            continue;
        }

        if (!tr->always_present) continue;
        if (!monsters || monsters->count <= 0) continue;

        g_txt_absent[txt]++;
        if (g_txt_absent[txt] >= 15)
            g_txt_dead[txt] = 1;
    }
}

bool boss_state_show_spawn(int boss_level) {
    uint32_t txt = txt_for_level(boss_level);
    if (txt == 0 || txt >= TXT_SLOTS) return true;
    return !g_txt_dead[txt];
}

bool boss_state_show_quest(int level_id, const char *quest_name) {
    int i;
    uint32_t txt;

    if (!quest_name || !quest_name[0]) return true;
    for (i = 0; QUEST_BOSS_LINK[i].level; i++) {
        if (QUEST_BOSS_LINK[i].level != level_id) continue;
        if (!str_ieq(QUEST_BOSS_LINK[i].quest_name, quest_name)) continue;
        txt = QUEST_BOSS_LINK[i].boss_txt;
        if (txt >= TXT_SLOTS) return true;
        return !g_txt_dead[txt];
    }
    return true;
}
