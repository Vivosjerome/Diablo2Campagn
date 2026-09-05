#include "memory.h"
#include "game.h"
#include "mapgen.h"
#include "mapdata.h"
#include "levelnames.h"
#include "elites.h"
#include "overlay.h"
#include "settings.h"
#include "bossstate.h"
#include "app.h"

#include <stdio.h>
#include <string.h>

#define MAX_REMOTE_BOSSES 8
#define MAX_REMOTE_GUIDES 4
#define IZUAL_LEVEL 105

static const int REMOTE_BOSS_LEVELS[] = { IZUAL_LEVEL, 0 };

/* local=1 : entree sur la map actuelle. local=0 : autre map (balise ou point), ligne via la sortie. */
typedef struct {
    int from;
    int dest;
    int local;
    const char *label;
} GuideRule;

static const GuideRule GUIDE_RULES[] = {
    /* Acte 1 */
    { 2, 3, 0, NULL },
    { 2, 8, 1, "Antre du Mal" },
    { 3, 4, 0, NULL },
    { 3, 17, 1, "Cimetiere" },
    { 4, 5, 0, NULL },
    { 4, 10, 1, "Passage souterrain" },
    { 5, 6, 0, NULL },
    { 6, 7, 0, NULL },
    { 6, 20, 1, "Tour oubliee" },
    { 7, 26, 0, "Monastere" },
    { 7, 12, 1, "Fosse" },
    { 26, 27, 0, NULL },
    { 27, 28, 0, NULL },
    { 28, 29, 0, NULL },
    { 29, 30, 0, NULL },
    { 30, 31, 0, NULL },
    { 31, 32, 0, NULL },
    { 32, 33, 0, NULL },
    { 33, 34, 0, NULL },
    { 34, 35, 0, NULL },
    { 35, 36, 0, NULL },
    { 36, 37, 0, NULL },

    /* Acte 2 — Couloirs des morts uniquement sur Collines arides (42) */
    { 41, 42, 0, NULL },
    { 42, 43, 0, NULL },
    { 42, 56, 1, "Couloirs des morts" },
    { 43, 44, 0, NULL },
    { 43, 62, 1, "Repaire du ver" },
    { 44, 45, 0, "Temple des viperes" },
    { 44, 46, 0, NULL },
    { 44, 65, 1, "Tunnels antiques" },
    { 45, 58, 1, "Temple des viperes" },
    { 56, 57, 1, NULL },
    { 57, 60, 1, NULL },
    { 58, 61, 1, NULL },
    { 62, 63, 1, NULL },
    { 63, 64, 1, NULL },

    /* Acte 3 */
    { 76, 78, 0, NULL },
    { 76, 77, 0, NULL },
    { 76, 85, 1, "Caverne de l'araignee" },
    { 77, 78, 0, NULL },
    { 78, 79, 0, NULL },
    { 78, 88, 1, "Donjon des Ecorches" },
    { 79, 80, 0, NULL },
    { 80, 81, 0, NULL },
    { 80, 94, 1, "Temple en ruine" },
    { 81, 82, 0, NULL },
    { 82, 83, 0, NULL },
    { 83, 100, 1, "Durance de la Haine" },
    { 100, 101, 1, NULL },
    { 101, 102, 1, NULL },

    /* Acte 4 — Izual reste le chemin depuis les Steppes (pas un guide de zone 105). */
    { 105, 106, 0, NULL },
    { 106, 107, 0, NULL },
    { 107, 108, 0, "Sanctuaire du Chaos" },

    /* Acte 5 */
    { 110, 111, 0, NULL },
    { 111, 112, 0, NULL },
    { 112, 113, 0, NULL },
    { 113, 115, 0, NULL },
    { 113, 114, 1, "Riviere gelee" },
    { 115, 117, 0, NULL },
    { 117, 118, 0, NULL },
    { 118, 120, 0, NULL },
    { 128, 129, 0, NULL },
    { 129, 130, 0, NULL },
    { 130, 131, 0, NULL },
};

static int add_campaign_guide(const char *json, int cur, const GuideRule *rule, RemoteGuide *out) {
    int is_wp = 0;
    char tmp[64];

    tmp[0] = 0;
    if (rule->local) {
        if (!map_zone_exit_world(json, cur, rule->dest,
                                 &out->world_x, &out->world_y, tmp, sizeof(tmp)))
            return 0;
        is_wp = 0;
    } else {
        if (!map_level_anchor_world(json, rule->dest,
                                    &out->world_x, &out->world_y, tmp, sizeof(tmp), &is_wp) &&
            !map_zone_exit_world(json, cur, rule->dest,
                                 &out->world_x, &out->world_y, tmp, sizeof(tmp)))
            return 0;
    }

    if (rule->label && rule->label[0])
        snprintf(out->label, sizeof(out->label), "%s", rule->label);
    else
        snprintf(out->label, sizeof(out->label), "%s", tmp[0] ? tmp : level_name(rule->dest));
    out->is_waypoint = is_wp;
    out->dest_level = rule->dest;
    return 1;
}

typedef struct {
    uint32_t seed;
    int level_id;
    float wx, wy;
    char label[64];
    int ok;
} BossSpawnCache;

static BossSpawnCache g_boss_spawn_cache[8];
static uint32_t g_boss_spawn_cache_seed;

static void boss_spawn_cache_reset(uint32_t seed) {
    if (g_boss_spawn_cache_seed == seed) return;
    memset(g_boss_spawn_cache, 0, sizeof(g_boss_spawn_cache));
    g_boss_spawn_cache_seed = seed;
}

static int collect_remote_bosses(const char *json, uint32_t seed, uint32_t level_id,
                                   RemoteBoss *out, int max_out) {
    int n = 0, bi;

    if (!json || !json[0] || level_is_town((int)level_id) || max_out <= 0)
        return 0;

    boss_spawn_cache_reset(seed);

    for (bi = 0; REMOTE_BOSS_LEVELS[bi] && n < max_out; bi++) {
        int bl = REMOTE_BOSS_LEVELS[bi];
        int ci = -1, j;

        if (bl == (int)level_id) continue;
        if (level_act(bl) != level_act((int)level_id)) continue;

        for (j = 0; j < 8; j++) {
            if (g_boss_spawn_cache[j].ok &&
                g_boss_spawn_cache[j].seed == seed &&
                g_boss_spawn_cache[j].level_id == bl) {
                ci = j;
                break;
            }
        }
        if (ci < 0) {
            for (j = 0; j < 8; j++) {
                if (!g_boss_spawn_cache[j].ok) {
                    ci = j;
                    break;
                }
            }
            if (ci < 0) ci = 0;
            g_boss_spawn_cache[ci].seed = seed;
            g_boss_spawn_cache[ci].level_id = bl;
            g_boss_spawn_cache[ci].ok = map_boss_spawn_world(
                json, bl, &g_boss_spawn_cache[ci].wx, &g_boss_spawn_cache[ci].wy,
                g_boss_spawn_cache[ci].label, sizeof(g_boss_spawn_cache[ci].label));
        }
        if (!g_boss_spawn_cache[ci].ok) continue;
        if (!boss_state_show_spawn(bl)) continue;

        out[n].world_x = g_boss_spawn_cache[ci].wx;
        out[n].world_y = g_boss_spawn_cache[ci].wy;
        out[n].boss_level = bl;
        snprintf(out[n].label, sizeof(out[n].label), "%s", g_boss_spawn_cache[ci].label);
        n++;
    }
    return n;
}

static int collect_campaign_guides(const char *json, uint32_t level_id,
                                   RemoteGuide *out, int max_out) {
    int cur = (int)level_id;
    int n = 0;
    int i, n_rules;

    if (!json || !json[0] || max_out <= 0)
        return 0;
    if (level_is_town(cur))
        return 0;

    n_rules = (int)(sizeof(GUIDE_RULES) / sizeof(GUIDE_RULES[0]));
    for (i = 0; i < n_rules && n < max_out; i++) {
        if (GUIDE_RULES[i].from != cur)
            continue;
        if (add_campaign_guide(json, cur, &GUIDE_RULES[i], &out[n]))
            n++;
    }
    return n;
}

int main(void) {
    D2Process d2;
    Overlay overlay;
    GameState st;
    MonList monsters;
    Settings settings;
    static LevelMap level;
    RemoteBoss remote_bosses[MAX_REMOTE_BOSSES];
    int remote_boss_count = 0;
    RemoteGuide remote_guides[MAX_REMOTE_GUIDES];
    int remote_guide_count = 0;
    char json_path[MAX_PATH];
    uint32_t last_seed = 0;
    uint32_t last_level = 0;
    uint64_t sticky = 0;
    DWORD last_monster_ms = 0;
    int hide_reason = 0;

    if (!d2_require_admin()) return 1;

    settings_init(&settings);

    if (!d2_attach(&d2)) return 1;
    app_log("attach ok");
    if (!overlay_init(&overlay, 500, 500)) {
        app_error("Initialisation overlay impossible.");
        d2_detach(&d2);
        return 1;
    }

    memset(&level, 0, sizeof(level));

    for (;;) {
        overlay_pump();
        settings_poll_hotkeys(&settings);
        if (overlay_want_quit()) break;

        if (!d2_is_foreground(d2.pid)) {
            if (hide_reason != 1) { hide_reason = 1; app_log("hide: D2R pas au premier plan"); }
            overlay_set_visible(&overlay, false);
            Sleep(200);
            continue;
        }

        /* Offset UI souvent faux apres un patch D2R : on se fie a la lecture unit/roster. */
        if (!game_read_state(&d2, &st, sticky) || !st.valid) {
            if (hide_reason != 3) { hide_reason = 3; app_log("hide: lecture etat / pos impossible"); }
            sticky = 0;
            overlay_set_visible(&overlay, false);
            if (last_seed != 0) {
                map_free(&level);
                boss_spawn_cache_reset(0);
                boss_state_reset(0);
                last_seed = 0;
                last_level = 0;
            }
            Sleep(200);
            continue;
        }
        sticky = st.unit;

        if (st.map_seed != last_seed) {
            map_free(&level);
            last_level = 0;
            remote_boss_count = 0;
            remote_guide_count = 0;
            memset(&monsters, 0, sizeof(monsters));
            last_monster_ms = 0;
            if (!mapgen_ensure(st.map_seed, st.difficulty, json_path, sizeof(json_path),
                               settings.allow_mapgen)) {
                if (hide_reason != 4) { hide_reason = 4; app_log("hide: mapgen / cache carte"); }
                overlay_set_visible(&overlay, false);
                Sleep(1000);
                continue;
            }
            level_names_load(json_path);
            boss_state_reset(st.map_seed);
            last_seed = st.map_seed;
        }

        if (st.level_id != last_level) {
            map_free(&level);
            if (!map_load_level_from_json_file(json_path, (int)st.level_id, &level)) {
                if (hide_reason != 5) { hide_reason = 5; app_log("hide: niveau json introuvable"); }
                overlay_set_visible(&overlay, false);
                Sleep(300);
                continue;
            }
            last_level = st.level_id;
            last_monster_ms = 0;
            memset(&monsters, 0, sizeof(monsters));
            remote_boss_count = collect_remote_bosses(json_path, st.map_seed, st.level_id,
                                                      remote_bosses, MAX_REMOTE_BOSSES);
            remote_guide_count = collect_campaign_guides(json_path, st.level_id,
                                                         remote_guides, MAX_REMOTE_GUIDES);
        }

        if (game_in_game(&d2) && game_ui_blocks_overlay(&d2)) {
            if (hide_reason != 6) { hide_reason = 6; app_log("hide: menu / inventaire ouvert"); }
            overlay_set_visible(&overlay, false);
            Sleep((DWORD)settings.read_interval_ms);
            continue;
        }

        if (hide_reason != 0) { hide_reason = 0; app_log("overlay visible"); }
        overlay_set_visible(&overlay, true);

        {
            DWORD now = GetTickCount();
            int in_town = level_is_town((int)st.level_id);

            if (in_town) {
                if (monsters.count != 0)
                    memset(&monsters, 0, sizeof(monsters));
            } else if (last_monster_ms == 0 ||
                       (int)(now - last_monster_ms) >= settings.monster_interval_ms) {
                monsters_read(&d2, st.level_id, &monsters);
                last_monster_ms = now;
            }
        }
        boss_state_update(st.level_id, &monsters, &d2);
        overlay_render(&overlay, &level, st.pos_x, st.pos_y, st.map_seed, st.difficulty,
                       st.level_id, st.name, &monsters, st.is_corpse, &settings,
                       remote_bosses, remote_boss_count,
                       remote_guides, remote_guide_count);
        Sleep((DWORD)settings.read_interval_ms);
    }

    overlay_shutdown(&overlay);
    d2_detach(&d2);
    return 0;
}
