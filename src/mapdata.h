#ifndef MYTURN_MAPDATA_H
#define MYTURN_MAPDATA_H

#include <stdint.h>
#include <stdbool.h>

#define MAP_MAX_W 1200
#define MAP_MAX_H 1200
#define MAP_MAX_POIS 256

typedef enum {
    POI_EXIT = 0,
    POI_GOOD_EXIT,
    POI_WAYPOINT,
    POI_PORTAL,
    POI_BOSS_SPAWN,
    POI_QUEST,
    POI_OTHER
} PoiKind;

typedef struct {
    PoiKind kind;
    int id;          /* exit target level id when type=exit */
    int x, y;        /* local map coords */
    int good;
    char label[64];
} MapPoi;

typedef struct {
    int id;
    int w, h;
    int offset_x, offset_y;
    uint8_t walk[MAP_MAX_H][MAP_MAX_W]; /* 1 = walkable */
    uint8_t edge[MAP_MAX_H][MAP_MAX_W]; /* 1 = wall outline */
    MapPoi pois[MAP_MAX_POIS];
    int poi_count;
    bool ready;
} LevelMap;

bool map_load_level_from_json_file(const char *path, int level_id, LevelMap *out);
bool map_boss_spawn_world(const char *path, int level_id, float *world_x, float *world_y,
                          char *label, size_t label_size);
bool map_waypoint_world(const char *path, int level_id, float *world_x, float *world_y,
                        char *label, size_t label_size);
bool map_level_anchor_world(const char *path, int level_id, float *world_x, float *world_y,
                            char *label, size_t label_size, int *is_waypoint);
bool map_zone_exit_world(const char *path, int level_id, int dest_level_id,
                         float *world_x, float *world_y, char *label, size_t label_size);
void map_free(LevelMap *m);

#endif
