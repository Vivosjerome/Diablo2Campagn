#include "elites.h"
#include "offsets.h"
#include <string.h>

#define OFF_NPC_TABLE_DELTA  (128ULL * 8ULL)
#define OFF_MON_FLAGS   0x1A

#define NPC_MODE_DEATH  0u
#define NPC_MODE_DEAD   12u

#define UNIT_READ_SIZE  0x1A7u
#define PATH_READ_SIZE  0x28u
#define STAT_HDR_SIZE   0xB8u
#define STAT_LIST_MAX   256u
#define STAT_ENTRY_SIZE 8u

/* StatEnum indices (D2R) */
#define STAT_DAMAGE_REDUCED   36
#define STAT_MAGIC_RESIST     37
#define STAT_FIRE_RESIST      39
#define STAT_LIGHT_RESIST     41
#define STAT_COLD_RESIST      43
#define STAT_POISON_RESIST    45

#define OFF_STAT_PTR      0x30
#define OFF_STAT_COUNT    0x38
#define OFF_STAT_UNIT_PTR 0x90
#define OFF_STAT_EX_PTR   0xA8
#define OFF_STAT_EX_COUNT 0xB0

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

static int scan_immunities_from_buf(const uint8_t *buf, uint32_t count) {
    uint32_t i, n = count > STAT_LIST_MAX ? STAT_LIST_MAX : count;
    int imm = 0;
    for (i = 0; i < n; i++) {
        uint16_t sid = u16_at(buf, (size_t)i * STAT_ENTRY_SIZE + 2);
        int16_t val = (int16_t)u16_at(buf, (size_t)i * STAT_ENTRY_SIZE + 4);
        if (val < 100) continue;
        switch (sid) {
        case STAT_FIRE_RESIST:   imm |= ELITE_IMM_FIRE; break;
        case STAT_LIGHT_RESIST:  imm |= ELITE_IMM_LIGHT; break;
        case STAT_COLD_RESIST:   imm |= ELITE_IMM_COLD; break;
        case STAT_POISON_RESIST: imm |= ELITE_IMM_POISON; break;
        case STAT_DAMAGE_REDUCED: imm |= ELITE_IMM_PHYS; break;
        case STAT_MAGIC_RESIST:  imm |= ELITE_IMM_MAGIC; break;
        default: break;
        }
    }
    return imm;
}

static int scan_immunities_from_list(const D2Process *p, uint64_t list_ptr, uint32_t count) {
    uint8_t buf[STAT_LIST_MAX * STAT_ENTRY_SIZE];
    uint32_t n = count > STAT_LIST_MAX ? STAT_LIST_MAX : count;
    if (!d2_is_ptr(list_ptr) || n == 0) return 0;
    if (!d2_read(p, (uintptr_t)list_ptr, buf, (size_t)n * STAT_ENTRY_SIZE)) return 0;
    return scan_immunities_from_buf(buf, n);
}

static int read_immunities(const D2Process *p, uint64_t pstats) {
    uint8_t shdr[STAT_HDR_SIZE];
    uint64_t stat_ptr, stat_ex_ptr, walk;
    uint32_t stat_count, stat_ex_count;
    int imm = 0;

    if (!d2_is_ptr(pstats)) return 0;
    if (!d2_read(p, (uintptr_t)pstats, shdr, STAT_HDR_SIZE)) return 0;

    stat_ptr = u64_at(shdr, OFF_STAT_PTR);
    stat_count = u32_at(shdr, OFF_STAT_COUNT);
    stat_ex_ptr = u64_at(shdr, OFF_STAT_EX_PTR);
    stat_ex_count = u32_at(shdr, OFF_STAT_EX_COUNT);

    walk = u64_at(shdr, OFF_STAT_UNIT_PTR);
    while (d2_is_ptr(walk)) {
        uint8_t node[0x40];
        uint64_t last_ptr;
        uint32_t last_cnt;
        if (!d2_read(p, (uintptr_t)walk, node, sizeof(node))) break;
        if (u64_at(node, 0x1C) & 0x40) break;
        walk = u64_at(node, 0x48);
        if (!d2_is_ptr(walk)) break;
        if (!d2_read(p, (uintptr_t)walk, node, sizeof(node))) break;
        last_ptr = u64_at(node, 0x30);
        last_cnt = u32_at(node, 0x38);
        imm |= scan_immunities_from_list(p, last_ptr, last_cnt);
    }

    imm |= scan_immunities_from_list(p, stat_ptr, stat_count);
    imm |= scan_immunities_from_list(p, stat_ex_ptr, stat_ex_count);
    return imm;
}

static bool id_in_list(uint32_t id, const uint16_t *list, int n) {
    int i;
    for (i = 0; i < n; i++)
        if (list[i] == id) return true;
    return false;
}

static const uint16_t TOWN_NPCS[] = {
    146,147,148,150,154,155,175,176,177,178,198,199,200,201,202,210,
    244,245,246,251,252,253,254,255,257,264,265,266,297,331,367,405,
    406,408,511,512,513,514,515,520,521,527
};

static const uint16_t PET_NPCS[] = {
    271,289,290,291,292,338,351,352,353,357,359,363,364,417,418,419,
    420,421,423,424,428,560,561
};

static bool is_town_npc(uint32_t txt) {
    return id_in_list(txt, TOWN_NPCS, (int)(sizeof(TOWN_NPCS) / sizeof(TOWN_NPCS[0])));
}

static bool is_pet_npc(uint32_t txt) {
    return id_in_list(txt, PET_NPCS, (int)(sizeof(PET_NPCS) / sizeof(PET_NPCS[0])));
}

static MonKind kind_from_flags(uint8_t flags) {
    if (flags == 10) return MON_SUPERUNIQUE;
    if (flags == 8)  return MON_UNIQUE;
    if (flags == 12) return MON_CHAMPION;
    if (flags == 16) return MON_MINION;
    return MON_NORMAL;
}

static bool is_act_boss(uint32_t txt) {
    switch (txt) {
    case 156: case 211: case 229: case 242: case 243: case 250: case 256:
    case 267: case 333: case 365: case 409: case 526: case 540: case 541:
    case 542: case 544: case 704: case 705: case 706: case 707: case 708:
    case 709:
        return true;
    default:
        return false;
    }
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

int boss_probe_on_level(const D2Process *p, uint32_t boss_level, uint32_t boss_txt) {
    uintptr_t table;
    uint64_t slots[D2_UNIT_TABLE_SLOTS];
    int slot;
    int found_alive = 0;
    int found_dead = 0;

    if (!p || !p->module_base || boss_txt == 0) return 0;

    table = p->module_base + p->off_unit_table + OFF_NPC_TABLE_DELTA;
    if (!d2_read_unit_table(p, table, slots)) return 0;

    for (slot = 0; slot < D2_UNIT_TABLE_SLOTS; slot++) {
        uint64_t unit = slots[slot];
        int hops = 0;
        while (d2_is_ptr(unit) && hops++ < 64) {
            uint8_t ubuf[UNIT_READ_SIZE];
            uint8_t pbuf[PATH_READ_SIZE];
            uint32_t mid, txt, mode;

            if (!d2_read(p, (uintptr_t)unit, ubuf, UNIT_READ_SIZE)) break;
            if (u32_at(ubuf, OFF_UNIT_TYPE) != 1) {
                unit = u64_at(ubuf, OFF_UNIT_PNEXT);
                continue;
            }
            if (!d2_read(p, u64_at(ubuf, OFF_UNIT_PPATH), pbuf, PATH_READ_SIZE)) {
                unit = u64_at(ubuf, OFF_UNIT_PNEXT);
                continue;
            }
            if (!level_id_from_path(p, pbuf, &mid) || mid != boss_level) {
                unit = u64_at(ubuf, OFF_UNIT_PNEXT);
                continue;
            }
            txt = u32_at(ubuf, OFF_UNIT_TXT);
            if (txt != boss_txt) {
                unit = u64_at(ubuf, OFF_UNIT_PNEXT);
                continue;
            }
            mode = u32_at(ubuf, OFF_UNIT_MODE);
            if (mode == NPC_MODE_DEATH || mode == NPC_MODE_DEAD || ubuf[OFF_UNIT_CORPSE])
                found_dead = 1;
            else
                found_alive = 1;
            unit = u64_at(ubuf, OFF_UNIT_PNEXT);
        }
    }
    if (found_alive) return 1;
    if (found_dead) return 2;
    return 0;
}

void monsters_read(const D2Process *p, uint32_t level_id, MonList *out) {
    uintptr_t table;
    uint64_t slots[D2_UNIT_TABLE_SLOTS];
    int slot;

    memset(out, 0, sizeof(*out));
    if (!p || !p->module_base) return;

    table = p->module_base + p->off_unit_table + OFF_NPC_TABLE_DELTA;
    if (!d2_read_unit_table(p, table, slots)) return;

    for (slot = 0; slot < D2_UNIT_TABLE_SLOTS && out->count < MAX_MONSTERS; slot++) {
        uint64_t unit = slots[slot];
        int hops = 0;
        while (d2_is_ptr(unit) && hops++ < 48 && out->count < MAX_MONSTERS) {
            uint8_t ubuf[UNIT_READ_SIZE];
            uint8_t pbuf[PATH_READ_SIZE];
            uint32_t mode, mid, txt;
            uint64_t pdata, ppath, pstats;
            uint8_t flags;
            MonUnit *m;

            if (!d2_read(p, (uintptr_t)unit, ubuf, UNIT_READ_SIZE)) break;

            if (u32_at(ubuf, OFF_UNIT_TYPE) != 1) {
                unit = u64_at(ubuf, OFF_UNIT_PNEXT);
                continue;
            }

            mode = u32_at(ubuf, OFF_UNIT_MODE);
            if (mode == NPC_MODE_DEATH || mode == NPC_MODE_DEAD) {
                unit = u64_at(ubuf, OFF_UNIT_PNEXT);
                continue;
            }

            pdata = u64_at(ubuf, OFF_UNIT_PDATA);
            ppath = u64_at(ubuf, OFF_UNIT_PPATH);
            pstats = u64_at(ubuf, OFF_UNIT_PSTATS);
            if (!d2_is_ptr(pdata) || !d2_is_ptr(ppath)) {
                unit = u64_at(ubuf, OFF_UNIT_PNEXT);
                continue;
            }

            if (!d2_read(p, (uintptr_t)ppath, pbuf, PATH_READ_SIZE)) {
                unit = u64_at(ubuf, OFF_UNIT_PNEXT);
                continue;
            }
            if (!level_id_from_path(p, pbuf, &mid)) {
                unit = u64_at(ubuf, OFF_UNIT_PNEXT);
                continue;
            }
            if (level_id >= 1 && level_id <= 136 && mid != level_id) {
                unit = u64_at(ubuf, OFF_UNIT_PNEXT);
                continue;
            }

            txt = u32_at(ubuf, OFF_UNIT_TXT);
            if (is_town_npc(txt) || is_pet_npc(txt)) {
                unit = u64_at(ubuf, OFF_UNIT_PNEXT);
                continue;
            }

            m = &out->list[out->count];
            m->txt = txt;
            if (is_act_boss(txt))
                m->kind = MON_BOSS;
            else {
                flags = 0;
                d2_read(p, (uintptr_t)pdata + OFF_MON_FLAGS, &flags, 1);
                m->kind = kind_from_flags(flags);
            }

            if (!path_pos_from_buf(pbuf, &m->x, &m->y)) {
                unit = u64_at(ubuf, OFF_UNIT_PNEXT);
                continue;
            }

            m->immunities = read_immunities(p, pstats);
            out->count++;
            unit = u64_at(ubuf, OFF_UNIT_PNEXT);
        }
    }
}
