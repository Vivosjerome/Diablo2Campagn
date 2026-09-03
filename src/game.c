#include "game.h"
#include "offsets.h"
#include "seed.h"
#include <string.h>
#include <stdio.h>

#define UNIT_READ_SIZE  0x1A7u
#define PATH_READ_SIZE  0x28u
#define ACTMISC_READ_SIZE 0x34u /* diff@0x830 .. end@0x860 from p_misc base */
#define ROSTER_READ_SIZE 0x150u

static uint32_t u32_at(const uint8_t *b, size_t off) {
    uint32_t v;
    memcpy(&v, b + off, sizeof(v));
    return v;
}

static uint64_t u64_at(const uint8_t *b, size_t off) {
    uint64_t v;
    memcpy(&v, b + off, sizeof(v));
    return v;
}

static uint16_t u16_at(const uint8_t *b, size_t off) {
    uint16_t v;
    memcpy(&v, b + off, sizeof(v));
    return v;
}

static void read_name(const D2Process *p, uint64_t pdata, char *out, size_t n) {
    char buf[48];
    size_t i;
    memset(out, 0, n);
    if (!d2_is_ptr(pdata)) return;
    if (!d2_read(p, (uintptr_t)pdata, buf, sizeof(buf))) return;
    for (i = 0; i < sizeof(buf) && i + 1 < n; i++) {
        if (buf[i] == 0) break;
        if ((unsigned char)buf[i] < 32 || (unsigned char)buf[i] > 126) break;
        out[i] = buf[i];
    }
    out[i] = 0;
}

static bool path_pos_from_buf(const uint8_t *path, float *ox, float *oy) {
    uint16_t off_x = u16_at(path, OFF_PATH_OFF_X);
    uint16_t dyn_x = u16_at(path, OFF_PATH_DYN_X);
    uint16_t off_y = u16_at(path, OFF_PATH_OFF_Y);
    uint16_t dyn_y = u16_at(path, OFF_PATH_DYN_Y);
    if (dyn_x == 0 && dyn_y == 0) return false;
    *ox = (float)dyn_x + (float)off_x / 65535.0f;
    *oy = (float)dyn_y + (float)off_y / 65535.0f;
    return true;
}

static bool level_id_from_path(const D2Process *p, const uint8_t *path, uint32_t *out_level) {
    uint64_t p_room, p_room2, p_level;
    p_room = u64_at(path, OFF_PATH_PROOM);
    if (!d2_is_ptr(p_room)) return false;
    p_room2 = d2_u64(p, (uintptr_t)p_room + OFF_ROOM_PROOM2);
    if (!d2_is_ptr(p_room2)) return false;
    p_level = d2_u64(p, (uintptr_t)p_room2 + OFF_ROOM2_PLEVEL);
    if (!d2_is_ptr(p_level)) return false;
    *out_level = d2_u32(p, (uintptr_t)p_level + OFF_LEVEL_ID);
    return true;
}

static bool roster_pos_for_unit(const D2Process *p, uint32_t unit_id, float *ox, float *oy, uint32_t *area) {
    uint64_t roster = d2_u64(p, p->module_base + (uintptr_t)OFF_ROSTER);
    uint8_t node[ROSTER_READ_SIZE];
    int hops = 0;

    while (d2_is_ptr(roster) && hops++ < 16) {
        uint32_t uid, px, py, ar;
        if (!d2_read(p, (uintptr_t)roster, node, ROSTER_READ_SIZE)) break;
        uid = u32_at(node, OFF_ROSTER_UNITID);
        px = u32_at(node, OFF_ROSTER_POS_X);
        py = u32_at(node, OFF_ROSTER_POS_Y);
        ar = u32_at(node, OFF_ROSTER_AREA);
        if (uid == unit_id && px > 0 && py > 0) {
            *ox = (float)px;
            *oy = (float)py;
            if (area) *area = ar;
            return true;
        }
        roster = u64_at(node, OFF_ROSTER_NEXT);
    }
    return false;
}

static bool roster_first(const D2Process *p, float *ox, float *oy, uint32_t *area,
                           uint32_t *unit_id, char *name, size_t nlen) {
    uint64_t roster = d2_u64(p, p->module_base + (uintptr_t)OFF_ROSTER);
    uint8_t node[ROSTER_READ_SIZE];
    uint32_t px, py;

    if (!d2_is_ptr(roster)) return false;
    if (!d2_read(p, (uintptr_t)roster, node, ROSTER_READ_SIZE)) return false;
    px = u32_at(node, OFF_ROSTER_POS_X);
    py = u32_at(node, OFF_ROSTER_POS_Y);
    if (px == 0 || py == 0) return false;
    *ox = (float)px;
    *oy = (float)py;
    if (area) *area = u32_at(node, OFF_ROSTER_AREA);
    if (unit_id) *unit_id = u32_at(node, OFF_ROSTER_UNITID);
    if (name && nlen) {
        size_t copy = nlen > 48 ? 48 : nlen;
        memcpy(name, node, copy);
        name[copy - 1] = 0;
    }
    return true;
}

static bool try_unit(const D2Process *p, uint64_t unit, GameState *out) {
    uint8_t ubuf[UNIT_READ_SIZE];
    uint8_t pbuf[PATH_READ_SIZE];
    uint8_t mbuf[ACTMISC_READ_SIZE];
    uint32_t utype, unit_id, diff, end_hash, level_id;
    uint64_t p_act, p_path, p_data, p_misc, init_hash;
    float px, py;

    if (!d2_is_ptr(unit)) return false;
    if (!d2_read(p, (uintptr_t)unit, ubuf, UNIT_READ_SIZE)) return false;

    utype = u32_at(ubuf, OFF_UNIT_TYPE);
    if (utype != 0) return false;

    unit_id = u32_at(ubuf, OFF_UNIT_ID);
    p_act = u64_at(ubuf, OFF_UNIT_PACT);
    p_path = u64_at(ubuf, OFF_UNIT_PPATH);
    p_data = u64_at(ubuf, OFF_UNIT_PDATA);
    if (!d2_is_ptr(p_act) || !d2_is_ptr(p_path)) return false;

    p_misc = d2_u64(p, (uintptr_t)p_act + OFF_ACT_PMISC);
    if (!d2_is_ptr(p_misc)) return false;
    if (!d2_read(p, (uintptr_t)p_misc + OFF_ACTMISC_DIFF, mbuf, ACTMISC_READ_SIZE)) return false;

    diff = u32_at(mbuf, 0);
    init_hash = u64_at(mbuf, OFF_ACTMISC_INIT - OFF_ACTMISC_DIFF);
    end_hash = u32_at(mbuf, OFF_ACTMISC_END - OFF_ACTMISC_DIFF);
    if (diff > 2 || init_hash == 0 || end_hash == 0) return false;

    if (!d2_read(p, (uintptr_t)p_path, pbuf, PATH_READ_SIZE)) return false;
    if (!level_id_from_path(p, pbuf, &level_id)) return false;
    if (level_id < 1 || level_id > 136) return false;
    if (!path_pos_from_buf(pbuf, &px, &py)) return false;

    {
        float rpx, rpy;
        uint32_t roster_area = 0;
        if (roster_pos_for_unit(p, unit_id, &rpx, &rpy, &roster_area)) {
            px = rpx;
            py = rpy;
            if (roster_area >= 1 && roster_area <= 136)
                level_id = roster_area;
        } else {
            float fx, fy;
            uint32_t fuid = 0, farea = 0;
            if (roster_first(p, &fx, &fy, &farea, &fuid, NULL, 0) && fuid == unit_id) {
                px = fx;
                py = fy;
                if (farea >= 1 && farea <= 136)
                    level_id = farea;
            }
        }
    }

    out->map_seed = seed_decrypt(init_hash, end_hash);
    out->difficulty = diff;
    out->level_id = level_id;
    out->pos_x = px;
    out->pos_y = py;
    out->unit = unit;
    out->is_corpse = ubuf[OFF_UNIT_CORPSE] != 0;
    {
        uint32_t mode = u32_at(ubuf, OFF_UNIT_MODE);
        if (mode == 0 || mode == 12)
            out->is_corpse = true;
    }
    read_name(p, p_data, out->name, sizeof(out->name));
    out->valid = out->map_seed != 0;
    return out->valid;
}

bool game_in_game(const D2Process *p) {
    if (!p || !p->module_base) return false;
    return d2_u8(p, p->module_base + (uintptr_t)OFF_UI_STATES) != 0;
}

bool game_read_state(const D2Process *p, GameState *out, uint64_t sticky_unit) {
    uintptr_t table;
    uint64_t slots[D2_UNIT_TABLE_SLOTS];
    int slot;
    memset(out, 0, sizeof(*out));
    if (!p || !p->module_base) return false;

    if (sticky_unit && try_unit(p, sticky_unit, out))
        return true;

    if (sticky_unit) {
        uint8_t ubuf[UNIT_READ_SIZE];
        uint8_t mbuf[ACTMISC_READ_SIZE];
        float rpx, rpy;
        uint32_t area = 0;
        char nbuf[48];
        uint64_t p_act;
        uint64_t p_misc;
        uint32_t diff, end_hash;
        uint64_t init_hash;

        if (!d2_read(p, (uintptr_t)sticky_unit, ubuf, UNIT_READ_SIZE)) goto scan_table;
        p_act = u64_at(ubuf, OFF_UNIT_PACT);
        if (!d2_is_ptr(p_act)) goto scan_table;
        p_misc = d2_u64(p, (uintptr_t)p_act + OFF_ACT_PMISC);
        if (!d2_is_ptr(p_misc)) goto scan_table;
        if (!d2_read(p, (uintptr_t)p_misc + OFF_ACTMISC_DIFF, mbuf, ACTMISC_READ_SIZE)) goto scan_table;
        diff = u32_at(mbuf, 0);
        init_hash = u64_at(mbuf, OFF_ACTMISC_INIT - OFF_ACTMISC_DIFF);
        end_hash = u32_at(mbuf, OFF_ACTMISC_END - OFF_ACTMISC_DIFF);
        if (diff <= 2 && init_hash && end_hash &&
            roster_first(p, &rpx, &rpy, &area, NULL, nbuf, sizeof(nbuf)) &&
            area >= 1 && area <= 136) {
            out->map_seed = seed_decrypt(init_hash, end_hash);
            out->difficulty = diff;
            out->level_id = area;
            out->pos_x = rpx;
            out->pos_y = rpy;
            out->unit = sticky_unit;
            out->is_corpse = ubuf[OFF_UNIT_CORPSE] != 0;
            strncpy(out->name, nbuf, sizeof(out->name) - 1);
            out->valid = out->map_seed != 0;
            if (out->valid) return true;
        }
    }

scan_table:
    table = p->module_base + (uintptr_t)OFF_UNIT_TABLE;
    if (!d2_read_unit_table(p, table, slots)) return false;

    for (slot = 0; slot < D2_UNIT_TABLE_SLOTS; slot++) {
        uint64_t unit = slots[slot];
        int hops = 0;
        while (d2_is_ptr(unit) && hops++ < 64) {
            uint8_t tail[16];
            if (try_unit(p, unit, out))
                return true;
            if (!d2_read(p, (uintptr_t)unit + OFF_UNIT_PNEXT, tail, sizeof(uint64_t))) break;
            unit = u64_at(tail, 0);
        }
    }
    return false;
}
