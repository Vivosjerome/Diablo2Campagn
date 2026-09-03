#ifndef MYTURN_SETTINGS_H
#define MYTURN_SETTINGS_H

#include <stdbool.h>

typedef struct {
    float map_opacity;        /* 0.05 .. 1.0 */
    int read_interval_ms;     /* player/state poll */
    int monster_interval_ms;  /* mob scan throttle */
    int allow_mapgen;         /* 0=cache only, 1=spawn d2-mapgen on miss */
} Settings;

void settings_init(Settings *s);
void settings_load(Settings *s);
void settings_save(const Settings *s);
void settings_poll_hotkeys(Settings *s);

#endif
