#ifndef MYTURN_GAME_H
#define MYTURN_GAME_H

#include "memory.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint32_t map_seed;
    uint32_t difficulty;
    uint32_t level_id;
    float pos_x;
    float pos_y;
    uint64_t unit;
    char name[64];
    bool valid;
    bool is_corpse;
} GameState;

bool game_in_game(const D2Process *p);

/* True si inventaire, sorts, stash, menu, etc. sont ouverts. */
bool game_ui_blocks_overlay(const D2Process *p);

/* Prefer sticky_unit if still valid (keeps same player each frame). */
bool game_read_state(const D2Process *p, GameState *out, uint64_t sticky_unit);

#endif
