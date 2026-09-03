#include "mapdata.h"
#include "levelnames.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void map_free(LevelMap *m) {
    if (m) memset(m, 0, sizeof(*m));
}

static void build_edges(LevelMap *m) {
    int y, x, w = m->w, h = m->h;
    memset(m->edge, 0, sizeof(m->edge));
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            int walk = m->walk[y][x];
            if (!walk) {
                /* non-walkable touching walkable = wall edge */
                int border = 0;
                int dy, dx;
                for (dy = -1; dy <= 1 && !border; dy++) {
                    for (dx = -1; dx <= 1; dx++) {
                        int nx = x + dx, ny = y + dy;
                        if (dx == 0 && dy == 0) continue;
                        if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
                        if (m->walk[ny][nx]) { border = 1; break; }
                    }
                }
                if (border) m->edge[y][x] = 1;
            } else if (x == 0 || y == 0 || x == w - 1 || y == h - 1) {
                m->edge[y][x] = 1;
            }
        }
    }
}

static int id_match_exact(const char *p, const char *key) {
    size_t n = strlen(key);
    if (strncmp(p, key, n) != 0) return 0;
    /* next char must NOT be a digit — avoids "id":1 matching "id":12 */
    return !isdigit((unsigned char)p[n]);
}

static const char *find_level_object(const char *json, int level_id) {
    /* Match only level headers: {"type":"map","id":N,"name":...}
       Never match exit/npc objects that reuse the same numeric id. */
    const char *p = json;
    char idkey[48];
    char idkey_sp[48];

    snprintf(idkey, sizeof(idkey), "\"id\":%d", level_id);
    snprintf(idkey_sp, sizeof(idkey_sp), "\"id\": %d", level_id);

    while (p && *p) {
        const char *a = strstr(p, "\"type\":\"map\"");
        const char *b = strstr(p, "\"type\": \"map\"");
        const char *typepos;
        const char *win_end;
        const char *q;
        const char *idpos = NULL;
        const char *mapk;

        if (!a && !b) break;
        typepos = (a && b) ? (a < b ? a : b) : (a ? a : b);
        win_end = typepos + 180;

        for (q = typepos; q && q < win_end; ) {
            const char *hit = strstr(q, idkey);
            const char *hit2 = strstr(q, idkey_sp);
            const char *best = NULL;
            const char *keyused = idkey;
            const char *namek;
            const char *offk;

            if (hit && hit < win_end) { best = hit; keyused = idkey; }
            if (hit2 && hit2 < win_end && (!best || hit2 < best)) {
                best = hit2;
                keyused = idkey_sp;
            }
            if (!best) break;
            if (!id_match_exact(best, keyused)) {
                q = best + 1;
                continue;
            }
            /* Level header has name + offset soon after id — exits do not. */
            namek = strstr(best, "\"name\"");
            offk = strstr(best, "\"offset\"");
            if (namek && namek < best + 100 && offk && offk < best + 200) {
                idpos = best;
                break;
            }
            q = best + 1;
        }

        if (idpos) {
            mapk = strstr(idpos, "\"map\":");
            if (mapk) {
                const char *next_hdr = strstr(idpos + 8, "\"type\":\"map\"");
                if (!next_hdr || next_hdr > mapk)
                    return idpos;
            }
        }
        p = typepos + 12;
    }
    return NULL;
}

static int parse_int_after_limited(const char *start, const char *end, const char *key, int *out) {
    const char *k;
    size_t region;
    if (!start || !end || end <= start) return 0;
    region = (size_t)(end - start);
    {
        char *tmp = (char *)malloc(region + 1);
        if (!tmp) return 0;
        memcpy(tmp, start, region);
        tmp[region] = 0;
        k = strstr(tmp, key);
        if (!k) { free(tmp); return 0; }
        k += strlen(key);
        while (*k && (isspace((unsigned char)*k) || *k == ':' || *k == '\"')) k++;
        *out = (int)strtol(k, NULL, 10);
        free(tmp);
        return 1;
    }
}

static void decode_rle_row(const int *runs, int nruns, uint8_t *row, int width) {
    int fill = 0;
    int x = 0;
    int i;
    memset(row, 0, (size_t)width);
    for (i = 0; i < nruns; i++) {
        int width_run = runs[i];
        int n;
        fill = !fill;
        if (!fill) {
            for (n = 0; n < width_run && x + n < width; n++)
                row[x + n] = 1;
        }
        x += width_run;
        if (i == nruns - 1 && fill) {
            int extra = width - x - 1;
            if (extra < 0) extra = 0;
            for (n = 0; n < extra && x + n < width; n++)
                row[x + n] = 1;
        }
    }
}

static const char *parse_int_list(const char *p, int *out, int maxn, int *count) {
    int n = 0;
    while (*p && *p != '[' ) p++;
    if (*p != '[') { *count = 0; return p; }
    p++;
    while (*p && *p != ']' && n < maxn) {
        while (*p && (isspace((unsigned char)*p) || *p == ',')) p++;
        if (*p == ']') break;
        if (*p == '-' || (*p >= '0' && *p <= '9')) {
            out[n++] = (int)strtol(p, (char **)&p, 10);
        } else break;
    }
    while (*p && *p != ']') p++;
    if (*p == ']') p++;
    *count = n;
    return p;
}

static int parse_int_field(const char *obj, const char *end, const char *key, int *out) {
    return parse_int_after_limited(obj, end, key, out);
}

static int parse_str_field(const char *obj, const char *end, const char *key, char *out, size_t n) {
    const char *k;
    size_t region;
    char *tmp;
    if (!obj || !end || end <= obj || n < 2) return 0;
    region = (size_t)(end - obj);
    tmp = (char *)malloc(region + 1);
    if (!tmp) return 0;
    memcpy(tmp, obj, region);
    tmp[region] = 0;
    k = strstr(tmp, key);
    if (!k) { free(tmp); return 0; }
    k += strlen(key);
    while (*k && *k != '\"') k++;
    if (*k != '\"') { free(tmp); return 0; }
    k++;
    {
        size_t i = 0;
        while (k[i] && k[i] != '\"' && i + 1 < n) {
            out[i] = k[i];
            i++;
        }
        out[i] = 0;
    }
    free(tmp);
    return 1;
}

static int str_ieq(const char *a, const char *b) {
    if (!a || !b) return 0;
    while (*a && *b) {
        int ca = tolower((unsigned char)*a++);
        int cb = tolower((unsigned char)*b++);
        if (ca != cb) return 0;
    }
    return *a == *b;
}

static int is_waypoint_obj(const char *name, const char *klass, int op) {
    if (op == 23) return 1;
    if (str_ieq(name, "Waypoint")) return 1;
    if (str_ieq(klass, "waypoint")) return 1;
    return 0;
}

static int is_portal_obj(const char *name, const char *klass) {
    if (str_ieq(klass, "portal")) return 1;
    if (str_ieq(name, "portal")) return 1;
    if (str_ieq(name, "Permanent Town Portal")) return 1;
    if (name && (strstr(name, "Portal") || strstr(name, "portal"))) return 1;
    return 0;
}

static int field_has(const char *obj, const char *end, const char *needle) {
    size_t region;
    char *tmp;
    int ok;
    if (!obj || !end || end <= obj) return 0;
    region = (size_t)(end - obj);
    tmp = (char *)malloc(region + 1);
    if (!tmp) return 0;
    memcpy(tmp, obj, region);
    tmp[region] = 0;
    ok = strstr(tmp, needle) != NULL;
    free(tmp);
    return ok;
}

/* Halls of Vaught (124): preset NPC on one side mirrors Nihlathak spawn. */
static int nihlathak_spawn_coords(int npc_x, int npc_y, int *out_x, int *out_y) {
    int x = npc_x, y = npc_y;
    if (x == 30 && y == 208) { x = 395; y = 210; }
    else if (x == 206 && y == 32) { x = 210; y = 395; }
    else if (x == 207 && y == 393) { x = 210; y = 25; }
    else if (x == 388 && y == 216) { x = 25; y = 210; }
    else return 0;
    *out_x = x;
    *out_y = y;
    return 1;
}

static int try_boss_poi(int level_id, const char *type, int id, int x, int y, MapPoi *poi) {
    if (strcmp(type, "npc") != 0) return 0;
    poi->x = x;
    poi->y = y;
    poi->kind = POI_BOSS_SPAWN;
    if (level_id == 33 && id == 743) {
        snprintf(poi->label, sizeof(poi->label), "Griswold");
        return 1;
    }
    if (level_id == 74 && id == 250) {
        snprintf(poi->label, sizeof(poi->label), "Invocateur");
        return 1;
    }
    if (level_id == 105) {
        snprintf(poi->label, sizeof(poi->label), "Izual");
        return 1;
    }
    if (level_id == 64) {
        snprintf(poi->label, sizeof(poi->label), "Duriel");
        return 1;
    }
    if (level_id == 49 && id == 744) {
        snprintf(poi->label, sizeof(poi->label), "Radament");
        return 1;
    }
    if (level_id == 124 && nihlathak_spawn_coords(x, y, &poi->x, &poi->y)) {
        snprintf(poi->label, sizeof(poi->label), "Nihlathak");
        return 1;
    }
    return 0;
}

static const char *quest_label_from_name(const char *name) {
    if (!name || !name[0]) return "Quete";
    if (str_ieq(name, "orifice")) return "Orifice";
    if (str_ieq(name, "gidbinn altar")) return "Gidbinn";
    if (str_ieq(name, "Hellforge")) return "Forge infernale";
    if (str_ieq(name, "cagedwussie1")) return "Anya";
    if (str_ieq(name, "Tome")) return "Tome";
    if (str_ieq(name, "LamTome")) return "Tome de Khalim";
    if (str_ieq(name, "Inifuss")) return "Inifuss";
    if (str_ieq(name, "taintedsunaltar")) return "Autel";
    if (str_ieq(name, "Seal")) return "Sceau";
    if (str_ieq(name, "StoneLambda")) return "Pierre Lambda";
    return name;
}

static int try_quest_poi(int level_id, const char *type, const char *name, MapPoi *poi) {
    if (level_id == 114 && strcmp(type, "npc") == 0) {
        poi->kind = POI_QUEST;
        snprintf(poi->label, sizeof(poi->label), "Anya");
        return 1;
    }
    if (level_id != 75 && name[0]) {
        if (str_ieq(name, "orifice") || str_ieq(name, "gidbinn altar") || str_ieq(name, "Hellforge") ||
            str_ieq(name, "cagedwussie1") || str_ieq(name, "Tome") || str_ieq(name, "LamTome") ||
            str_ieq(name, "Inifuss") || str_ieq(name, "taintedsunaltar") || str_ieq(name, "Seal") ||
            str_ieq(name, "StoneLambda")) {
            poi->kind = POI_QUEST;
            snprintf(poi->label, sizeof(poi->label), "%s", quest_label_from_name(name));
            return 1;
        }
    }
    if ((level_id == 84 || level_id == 85 || level_id == 91) && str_ieq(name, "chest")) {
        poi->kind = POI_QUEST;
        snprintf(poi->label, sizeof(poi->label), "Coffre quete");
        return 1;
    }
    return 0;
}

static void parse_objects(const char *lvl, int level_id, LevelMap *out) {
    const char *objs;
    const char *mapk;
    const char *p;
    out->poi_count = 0;
    objs = strstr(lvl, "\"objects\"");
    if (!objs) return;
    mapk = strstr(objs, "\"map\"");
    if (!mapk) mapk = objs + 50000;
    p = strchr(objs, '[');
    if (!p || p > mapk) return;
    p++;
    while (p < mapk && out->poi_count < MAP_MAX_POIS) {
        const char *start;
        const char *end;
        int x = 0, y = 0, id = 0, op = 0;
        char type[32], name[48], klass[48];
        MapPoi *poi;
        int interesting = 0;

        while (p < mapk && *p != '{') p++;
        if (p >= mapk || *p != '{') break;
        start = p;
        end = start + 1;
        {
            int depth = 1;
            while (end < mapk && depth > 0) {
                if (*end == '{') depth++;
                else if (*end == '}') depth--;
                end++;
            }
        }
        type[0] = name[0] = klass[0] = 0;
        parse_str_field(start, end, "\"type\"", type, sizeof(type));
        parse_str_field(start, end, "\"name\"", name, sizeof(name));
        parse_str_field(start, end, "\"class\"", klass, sizeof(klass));
        parse_int_field(start, end, "\"id\"", &id);
        parse_int_field(start, end, "\"x\"", &x);
        parse_int_field(start, end, "\"y\"", &y);
        parse_int_field(start, end, "\"op\"", &op);

        poi = &out->pois[out->poi_count];
        memset(poi, 0, sizeof(*poi));
        poi->id = id;
        poi->x = x;
        poi->y = y;

        if (strcmp(type, "exit") == 0) {
            int good = field_has(start, end, "\"isGoodExit\":true") ||
                       field_has(start, end, "\"isGoodExit\": true");
            /* Canyon des Mages : les 6 faux tombeaux ne sont pas affiches. */
            if (level_id == 46 && id >= 66 && id <= 72 && !good) {
                p = end;
                continue;
            }
            poi->kind = good ? POI_GOOD_EXIT : POI_EXIT;
            if (level_id == 46 && good && id >= 66 && id <= 72)
                snprintf(poi->label, sizeof(poi->label), "Tombeau de Tal Rasha");
            else
                snprintf(poi->label, sizeof(poi->label), "%s", level_name(id));
            interesting = 1;
        } else if (is_waypoint_obj(name, klass, op)) {
            poi->kind = POI_WAYPOINT;
            snprintf(poi->label, sizeof(poi->label), "Balise");
            interesting = 1;
        } else if (is_portal_obj(name, klass)) {
            poi->kind = POI_PORTAL;
            snprintf(poi->label, sizeof(poi->label), "Portail");
            interesting = 1;
        } else if (try_boss_poi(level_id, type, id, x, y, poi)) {
            interesting = 1;
        } else if (try_quest_poi(level_id, type, name, poi)) {
            interesting = 1;
        }

        if (interesting && poi->x >= 0 && poi->y >= 0)
            out->poi_count++;
        p = end;
    }
}

bool map_load_level_from_json_file(const char *path, int level_id, LevelMap *out) {
    FILE *f;
    long sz;
    char *buf;
    const char *lvl;
    const char *mapk;
    const char *off;
    const char *szobj;
    int w = 0, h = 0, ox = 0, oy = 0;
    int runs[1024];
    int y;

    memset(out, 0, sizeof(*out));
    f = fopen(path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 80 * 1024 * 1024) { fclose(f); return false; }
    buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return false; }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) { free(buf); fclose(f); return false; }
    buf[sz] = 0;
    fclose(f);

    lvl = find_level_object(buf, level_id);
    if (!lvl) { free(buf); return false; }

    off = strstr(lvl, "\"offset\"");
    szobj = strstr(lvl, "\"size\"");
    if (off && szobj && szobj > off) {
        parse_int_after_limited(off, szobj, "\"x\"", &ox);
        parse_int_after_limited(off, szobj, "\"y\"", &oy);
    }
    if (szobj) {
        const char *after = strstr(szobj, "\"objects\"");
        if (!after) after = strstr(szobj, "\"map\"");
        if (!after) after = szobj + 80;
        parse_int_after_limited(szobj, after, "\"width\"", &w);
        parse_int_after_limited(szobj, after, "\"height\"", &h);
    }
    if (w <= 0 || h <= 0 || w > MAP_MAX_W || h > MAP_MAX_H) {
        free(buf);
        return false;
    }

    parse_objects(lvl, level_id, out);

    mapk = strstr(lvl, "\"map\":");
    if (!mapk) { free(buf); return false; }
    mapk = strchr(mapk, '[');
    if (!mapk) { free(buf); return false; }
    mapk++;

    for (y = 0; y < h; y++) {
        int nruns = 0;
        while (*mapk && isspace((unsigned char)*mapk)) mapk++;
        if (*mapk == ']') break;
        if (*mapk != '[') {
            if (*mapk == ',') { mapk++; y--; continue; }
            break;
        }
        mapk = parse_int_list(mapk, runs, 1024, &nruns);
        decode_rle_row(runs, nruns, out->walk[y], w);
        while (*mapk && (*mapk == ',' || isspace((unsigned char)*mapk))) mapk++;
    }

    out->id = level_id;
    out->w = w;
    out->h = h;
    out->offset_x = ox;
    out->offset_y = oy;
    build_edges(out);
    out->ready = true;
    free(buf);
    return true;
}

bool map_boss_spawn_world(const char *path, int level_id, float *world_x, float *world_y,
                          char *label, size_t label_size) {
    FILE *f;
    long sz;
    char *buf;
    const char *lvl;
    const char *off, *szobj;
    LevelMap *tmp;
    int ox = 0, oy = 0, i;

    tmp = (LevelMap *)malloc(sizeof(LevelMap));
    if (!tmp) return false;
    memset(tmp, 0, sizeof(*tmp));

    f = fopen(path, "rb");
    if (!f) { free(tmp); return false; }
    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 80 * 1024 * 1024) { fclose(f); free(tmp); return false; }
    buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); free(tmp); return false; }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) { free(buf); fclose(f); free(tmp); return false; }
    buf[sz] = 0;
    fclose(f);

    lvl = find_level_object(buf, level_id);
    if (!lvl) { free(buf); free(tmp); return false; }

    off = strstr(lvl, "\"offset\"");
    szobj = strstr(lvl, "\"size\"");
    if (off && szobj && szobj > off) {
        parse_int_after_limited(off, szobj, "\"x\"", &ox);
        parse_int_after_limited(off, szobj, "\"y\"", &oy);
    }

    parse_objects(lvl, level_id, tmp);
    tmp->offset_x = ox;
    tmp->offset_y = oy;
    free(buf);

    for (i = 0; i < tmp->poi_count; i++) {
        if (tmp->pois[i].kind == POI_BOSS_SPAWN) {
            if (world_x) { *world_x = (float)tmp->pois[i].x + (float)ox; }
            if (world_y) { *world_y = (float)tmp->pois[i].y + (float)oy; }
            if (label && label_size > 0)
                snprintf(label, label_size, "%s", tmp->pois[i].label);
            free(tmp);
            return true;
        }
    }
    free(tmp);
    return false;
}

bool map_waypoint_world(const char *path, int level_id, float *world_x, float *world_y,
                        char *label, size_t label_size) {
    FILE *f;
    long sz;
    char *buf;
    const char *lvl;
    const char *off, *szobj;
    LevelMap *tmp;
    int ox = 0, oy = 0, i;

    tmp = (LevelMap *)malloc(sizeof(LevelMap));
    if (!tmp) return false;
    memset(tmp, 0, sizeof(*tmp));

    f = fopen(path, "rb");
    if (!f) { free(tmp); return false; }
    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 80 * 1024 * 1024) { fclose(f); free(tmp); return false; }
    buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); free(tmp); return false; }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) { free(buf); fclose(f); free(tmp); return false; }
    buf[sz] = 0;
    fclose(f);

    lvl = find_level_object(buf, level_id);
    if (!lvl) { free(buf); free(tmp); return false; }

    off = strstr(lvl, "\"offset\"");
    szobj = strstr(lvl, "\"size\"");
    if (off && szobj && szobj > off) {
        parse_int_after_limited(off, szobj, "\"x\"", &ox);
        parse_int_after_limited(off, szobj, "\"y\"", &oy);
    }

    parse_objects(lvl, level_id, tmp);
    tmp->offset_x = ox;
    tmp->offset_y = oy;
    free(buf);

    for (i = 0; i < tmp->poi_count; i++) {
        if (tmp->pois[i].kind == POI_WAYPOINT) {
            if (world_x) *world_x = (float)tmp->pois[i].x + (float)ox;
            if (world_y) *world_y = (float)tmp->pois[i].y + (float)oy;
            if (label && label_size > 0)
                snprintf(label, label_size, "Balise");
            free(tmp);
            return true;
        }
    }
    free(tmp);
    return false;
}

bool map_level_anchor_world(const char *path, int level_id, float *world_x, float *world_y,
                            char *label, size_t label_size, int *is_waypoint) {
    FILE *f;
    long sz;
    char *buf;
    const char *lvl;
    const char *off, *szobj;
    LevelMap *tmp;
    int ox = 0, oy = 0, i;
    int exit_x = 0, exit_y = 0, have_exit = 0;

    if (is_waypoint) *is_waypoint = 0;
    tmp = (LevelMap *)malloc(sizeof(LevelMap));
    if (!tmp) return false;
    memset(tmp, 0, sizeof(*tmp));

    f = fopen(path, "rb");
    if (!f) { free(tmp); return false; }
    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 80 * 1024 * 1024) { fclose(f); free(tmp); return false; }
    buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); free(tmp); return false; }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) { free(buf); fclose(f); free(tmp); return false; }
    buf[sz] = 0;
    fclose(f);

    lvl = find_level_object(buf, level_id);
    if (!lvl) { free(buf); free(tmp); return false; }

    off = strstr(lvl, "\"offset\"");
    szobj = strstr(lvl, "\"size\"");
    if (off && szobj && szobj > off) {
        parse_int_after_limited(off, szobj, "\"x\"", &ox);
        parse_int_after_limited(off, szobj, "\"y\"", &oy);
    }

    parse_objects(lvl, level_id, tmp);
    free(buf);

    for (i = 0; i < tmp->poi_count; i++) {
        if (tmp->pois[i].kind == POI_WAYPOINT) {
            if (world_x) *world_x = (float)tmp->pois[i].x + (float)ox;
            if (world_y) *world_y = (float)tmp->pois[i].y + (float)oy;
            if (label && label_size > 0)
                snprintf(label, label_size, "Balise");
            if (is_waypoint) *is_waypoint = 1;
            free(tmp);
            return true;
        }
    }
    for (i = 0; i < tmp->poi_count; i++) {
        if (tmp->pois[i].kind != POI_EXIT && tmp->pois[i].kind != POI_GOOD_EXIT) continue;
        exit_x = tmp->pois[i].x;
        exit_y = tmp->pois[i].y;
        have_exit = 1;
        if (tmp->pois[i].kind == POI_GOOD_EXIT) break;
    }
    if (have_exit) {
        if (world_x) *world_x = (float)exit_x + (float)ox;
        if (world_y) *world_y = (float)exit_y + (float)oy;
        if (label && label_size > 0)
            snprintf(label, label_size, "%s", level_name(level_id));
        free(tmp);
        return true;
    }
    if (world_x) *world_x = (float)ox + 8.0f;
    if (world_y) *world_y = (float)oy + 8.0f;
    if (label && label_size > 0)
        snprintf(label, label_size, "%s", level_name(level_id));
    free(tmp);
    return true;
}

bool map_zone_exit_world(const char *path, int level_id, int dest_level_id,
                         float *world_x, float *world_y, char *label, size_t label_size) {
    FILE *f;
    long sz;
    char *buf;
    const char *lvl;
    const char *off, *szobj;
    LevelMap *tmp;
    int ox = 0, oy = 0, i;
    int found = 0;
    int best_x = 0, best_y = 0;
    int best_good = 0;
    char best_label[64];

    tmp = (LevelMap *)malloc(sizeof(LevelMap));
    if (!tmp) return false;
    memset(tmp, 0, sizeof(*tmp));
    best_label[0] = 0;

    f = fopen(path, "rb");
    if (!f) { free(tmp); return false; }
    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 80 * 1024 * 1024) { fclose(f); free(tmp); return false; }
    buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); free(tmp); return false; }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) { free(buf); fclose(f); free(tmp); return false; }
    buf[sz] = 0;
    fclose(f);

    lvl = find_level_object(buf, level_id);
    if (!lvl) { free(buf); free(tmp); return false; }

    off = strstr(lvl, "\"offset\"");
    szobj = strstr(lvl, "\"size\"");
    if (off && szobj && szobj > off) {
        parse_int_after_limited(off, szobj, "\"x\"", &ox);
        parse_int_after_limited(off, szobj, "\"y\"", &oy);
    }

    parse_objects(lvl, level_id, tmp);
    tmp->offset_x = ox;
    tmp->offset_y = oy;
    free(buf);

    for (i = 0; i < tmp->poi_count; i++) {
        const MapPoi *poi = &tmp->pois[i];
        if (poi->kind != POI_EXIT && poi->kind != POI_GOOD_EXIT) continue;
        if (poi->id != dest_level_id) continue;
        if (found && best_good && poi->kind != POI_GOOD_EXIT) continue;
        best_x = poi->x;
        best_y = poi->y;
        snprintf(best_label, sizeof(best_label), "%s", poi->label[0] ? poi->label : level_name(dest_level_id));
        best_good = (poi->kind == POI_GOOD_EXIT);
        found = 1;
        if (best_good) break;
    }

    if (!found) { free(tmp); return false; }
    if (world_x) *world_x = (float)best_x + (float)ox;
    if (world_y) *world_y = (float)best_y + (float)oy;
    if (label && label_size > 0)
        snprintf(label, label_size, "%s", best_label);
    free(tmp);
    return true;
}
