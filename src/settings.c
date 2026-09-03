#include "settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define SETTINGS_FILE "config.ini"
#define OPACITY_MIN 0.05f
#define OPACITY_MAX 1.0f
#define OPACITY_STEP 0.05f
#define READ_INTERVAL_MIN 20
#define READ_INTERVAL_MAX 500
#define MONSTER_INTERVAL_MIN 100
#define MONSTER_INTERVAL_MAX 2000

static int g_minus_held;
static int g_plus_held;

static int minus_down(void) {
    return (GetAsyncKeyState(VK_OEM_MINUS) & 0x8000) != 0 ||
           (GetAsyncKeyState(VK_SUBTRACT) & 0x8000) != 0;
}

static int plus_down(void) {
    return (GetAsyncKeyState(VK_OEM_PLUS) & 0x8000) != 0 ||
           (GetAsyncKeyState(VK_ADD) & 0x8000) != 0;
}

static void settings_path(char *out, size_t n) {
    char dir[MAX_PATH];
    GetModuleFileNameA(NULL, dir, MAX_PATH);
    {
        char *slash = strrchr(dir, '\\');
        if (slash) *slash = 0;
    }
    snprintf(out, n, "%s\\%s", dir, SETTINGS_FILE);
}

static int clamp_int(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

void settings_init(Settings *s) {
    s->map_opacity = 0.85f;
    s->read_interval_ms = 80;
    s->monster_interval_ms = 500;
    s->allow_mapgen = 1;
    settings_load(s);
}

void settings_load(Settings *s) {
    char path[MAX_PATH];
    FILE *f;
    char line[128];
    float v;
    int iv;

    settings_path(path, sizeof(path));
    f = fopen(path, "r");
    if (!f) return;
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, " map_opacity = %f", &v) == 1 ||
            sscanf(line, "map_opacity=%f", &v) == 1) {
            if (v < OPACITY_MIN) v = OPACITY_MIN;
            if (v > OPACITY_MAX) v = OPACITY_MAX;
            s->map_opacity = v;
        } else if (sscanf(line, " read_interval_ms = %d", &iv) == 1 ||
                   sscanf(line, "read_interval_ms=%d", &iv) == 1) {
            s->read_interval_ms = clamp_int(iv, READ_INTERVAL_MIN, READ_INTERVAL_MAX);
        } else if (sscanf(line, " monster_interval_ms = %d", &iv) == 1 ||
                   sscanf(line, "monster_interval_ms=%d", &iv) == 1) {
            s->monster_interval_ms = clamp_int(iv, MONSTER_INTERVAL_MIN, MONSTER_INTERVAL_MAX);
        } else if (sscanf(line, " allow_mapgen = %d", &iv) == 1 ||
                   sscanf(line, "allow_mapgen=%d", &iv) == 1) {
            s->allow_mapgen = iv ? 1 : 0;
        }
    }
    fclose(f);
}

void settings_save(const Settings *s) {
    char path[MAX_PATH];
    FILE *f;

    settings_path(path, sizeof(path));
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "map_opacity=%.2f\n", s->map_opacity);
    fprintf(f, "read_interval_ms=%d\n", s->read_interval_ms);
    fprintf(f, "monster_interval_ms=%d\n", s->monster_interval_ms);
    fprintf(f, "allow_mapgen=%d\n", s->allow_mapgen);
    fclose(f);
}

static int key_edge(int down, int *held) {
    int edge = down && !*held;
    *held = down;
    return edge;
}

void settings_poll_hotkeys(Settings *s) {
    float before = s->map_opacity;

    if (key_edge(minus_down(), &g_minus_held)) {
        s->map_opacity -= OPACITY_STEP;
        if (s->map_opacity < OPACITY_MIN) s->map_opacity = OPACITY_MIN;
    }
    if (key_edge(plus_down(), &g_plus_held)) {
        s->map_opacity += OPACITY_STEP;
        if (s->map_opacity > OPACITY_MAX) s->map_opacity = OPACITY_MAX;
    }

    if (s->map_opacity != before)
        settings_save(s);
}
