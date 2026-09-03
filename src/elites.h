#ifndef MYTURN_ELITES_H
#define MYTURN_ELITES_H

#include "memory.h"
#include <stdint.h>

#define MAX_MONSTERS 192

#define ELITE_IMM_FIRE    0x01
#define ELITE_IMM_LIGHT   0x02
#define ELITE_IMM_COLD    0x04
#define ELITE_IMM_POISON  0x08
#define ELITE_IMM_PHYS    0x10
#define ELITE_IMM_MAGIC   0x20

typedef enum {
    MON_NORMAL = 0,
    MON_MINION,
    MON_CHAMPION,
    MON_UNIQUE,
    MON_SUPERUNIQUE,
    MON_BOSS
} MonKind;

typedef struct {
    MonKind kind;
    float x, y;
    int immunities; /* ELITE_IMM_* bitmask */
    uint32_t txt;
} MonUnit;

typedef struct {
    MonUnit list[MAX_MONSTERS];
    int count;
} MonList;

/* legacy aliases */
typedef MonKind EliteKind;
typedef MonUnit EliteUnit;
typedef MonList EliteList;
#define ELITE_NORMAL MON_NORMAL
#define ELITE_MINION MON_MINION
#define ELITE_CHAMPION MON_CHAMPION
#define ELITE_UNIQUE MON_UNIQUE
#define ELITE_SUPERUNIQUE MON_SUPERUNIQUE
#define ELITE_BOSS MON_BOSS
#define MAX_ELITES MAX_MONSTERS

void monsters_read(const D2Process *p, uint32_t level_id, MonList *out);
#define elites_read monsters_read

/* 0=absent, 1=alive, 2=dead/corpse on boss_level */
int boss_probe_on_level(const D2Process *p, uint32_t boss_level, uint32_t boss_txt);

#endif
