#ifndef MYTURN_MEMORY_H
#define MYTURN_MEMORY_H

#include <windows.h>
#include <stdint.h>
#include <stdbool.h>

#define D2_UNIT_TABLE_SLOTS 128

typedef struct {
    HANDLE process;
    DWORD pid;
    uintptr_t module_base;
    uintptr_t off_unit_table;
    uintptr_t off_roster;
    uintptr_t off_ui_states;
} D2Process;

bool d2_attach(D2Process *out);
void d2_detach(D2Process *p);
bool d2_read(const D2Process *p, uintptr_t addr, void *buf, size_t len);
bool d2_read_unit_table(const D2Process *p, uintptr_t table_addr, uint64_t slots[D2_UNIT_TABLE_SLOTS]);

static inline uint32_t d2_u32(const D2Process *p, uintptr_t addr) {
    uint32_t v = 0;
    d2_read(p, addr, &v, sizeof(v));
    return v;
}

static inline uint64_t d2_u64(const D2Process *p, uintptr_t addr) {
    uint64_t v = 0;
    d2_read(p, addr, &v, sizeof(v));
    return v;
}

static inline uint16_t d2_u16(const D2Process *p, uintptr_t addr) {
    uint16_t v = 0;
    d2_read(p, addr, &v, sizeof(v));
    return v;
}

static inline uint8_t d2_u8(const D2Process *p, uintptr_t addr) {
    uint8_t v = 0;
    d2_read(p, addr, &v, sizeof(v));
    return v;
}

bool d2_is_ptr(uint64_t a);

/* Quitte le process si pas lance en Administrateur. */
bool d2_require_admin(void);

/* True si D2R.exe a le focus clavier (fenetre active). */
bool d2_is_foreground(DWORD pid);

/* False si D2R s'est ferme. */
bool d2_still_running(const D2Process *p);

#endif
