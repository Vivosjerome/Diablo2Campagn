#include "seed.h"

uint32_t seed_decrypt(uint64_t init_hash, uint32_t end_hash) {
    const uint64_t magic = 0x6AC690C5ULL;
    const uint64_t offset = 666ULL;
    const uint64_t divisor = 65536ULL;
    uint64_t modifier = 0;
    uint64_t try_seed;
    uint32_t i;
    (void)init_hash;

    for (try_seed = 0; try_seed < divisor; try_seed++) {
        uint64_t seed_result = (try_seed * magic + offset) & 0xFFFFFFFFULL;
        if (seed_result % divisor == (uint64_t)end_hash % divisor)
            modifier = try_seed;
    }
    for (i = 0; i < (uint32_t)divisor; i++) {
        try_seed = modifier + (uint64_t)i * divisor;
        {
            uint64_t seed_result = (try_seed * magic + offset) & 0xFFFFFFFFULL;
            if (seed_result == (uint64_t)end_hash)
                return (uint32_t)(try_seed & 0xFFFFFFFFULL);
        }
    }
    return 0;
}
