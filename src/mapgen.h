#ifndef MYTURN_MAPGEN_H
#define MYTURN_MAPGEN_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

bool mapgen_ensure(uint32_t seed, uint32_t difficulty, char *out_json_path, size_t path_len,
                   int allow_mapgen);

#endif
