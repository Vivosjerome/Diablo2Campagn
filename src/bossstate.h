#ifndef MYTURN_BOSSSTATE_H
#define MYTURN_BOSSSTATE_H

#include "elites.h"
#include <stdbool.h>
#include <stdint.h>

void boss_state_reset(uint32_t seed);
void boss_state_update(uint32_t level_id, const MonList *monsters, const D2Process *p);
bool boss_state_show_spawn(int boss_level);
bool boss_state_show_quest(int level_id, const char *quest_name);

#endif
