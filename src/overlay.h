#ifndef MYTURN_OVERLAY_H
#define MYTURN_OVERLAY_H

#include "mapdata.h"
#include "elites.h"
#include "settings.h"
#include <windows.h>
#include <stdbool.h>

typedef struct {
    HWND hwnd;
    int win_w, win_h;
    HBITMAP dib;
    void *bits;
    int dib_w, dib_h;
    int anchor_x, anchor_y;
    int visible;
} Overlay;

typedef struct {
    float world_x, world_y;
    char label[64];
    int boss_level;
} RemoteBoss;

typedef struct {
    float world_x, world_y;
    char label[64];
    int is_waypoint;
    int dest_level;
} RemoteGuide;

bool overlay_init(Overlay *o, int w, int h);
void overlay_shutdown(Overlay *o);
void overlay_set_visible(Overlay *o, bool show);
void overlay_anchor_topright(Overlay *o);
void overlay_render(Overlay *o, const LevelMap *map, float player_world_x, float player_world_y,
                    uint32_t seed, uint32_t diff, uint32_t level_id, const char *name,
                    const MonList *monsters, bool player_is_corpse, const Settings *settings,
                    const RemoteBoss *remote_bosses, int remote_boss_count,
                    const RemoteGuide *remote_guides, int remote_guide_count);
void overlay_pump(void);
int overlay_want_quit(void);

#endif
