#include "overlay.h"
#include "elites.h"
#include "settings.h"
#include "bossstate.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MAP_SCALE 2.2f
#define COLKEY RGB(0, 0, 0)
#define ISO_C 0.70710678f
#define ANCHOR_MARGIN_X 8
#define ANCHOR_MARGIN_Y 0
#define OVERLAY_CLASS "ShellFrameHost"
#define OVERLAY_TITLE " "
#define HOTKEY_QUIT_F12  1
#define HOTKEY_QUIT_END  2
#ifndef MOD_NOREPEAT
#define MOD_NOREPEAT 0x4000
#endif

typedef BOOL (__stdcall *PFN_SetLayeredWindowAttributes)(HWND, COLORREF, BYTE, DWORD);
static PFN_SetLayeredWindowAttributes pSetLayeredWindowAttributes;
static HFONT g_font_poi;
static volatile int g_quit_requested;

static void load_layered(void) {
    HMODULE u;
    if (pSetLayeredWindowAttributes) return;
    u = GetModuleHandleA("user32.dll");
    if (u) pSetLayeredWindowAttributes = (PFN_SetLayeredWindowAttributes)GetProcAddress(u, "SetLayeredWindowAttributes");
}

static void ensure_fonts(void) {
    if (g_font_poi) return;
    g_font_poi = CreateFontA(15, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET,
                             OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_SWISS, "Trebuchet MS");
}

static Overlay *g_overlay_from_hwnd(HWND hwnd) {
    return (Overlay *)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
}

void overlay_anchor_topright(Overlay *o) {
    int sw, x, y;
    if (!o || !o->hwnd) return;
    sw = GetSystemMetrics(SM_CXSCREEN);
    x = sw - o->win_w - ANCHOR_MARGIN_X;
    y = ANCHOR_MARGIN_Y;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    o->anchor_x = x;
    o->anchor_y = y;
    SetWindowPos(o->hwnd, HWND_TOPMOST, x, y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
}

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Overlay *o = g_overlay_from_hwnd(hwnd);

    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_HOTKEY:
        if (wParam == HOTKEY_QUIT_F12 || wParam == HOTKEY_QUIT_END)
            PostQuitMessage(0);
        return 0;
    case WM_NCHITTEST:
        return HTTRANSPARENT;
    case WM_WINDOWPOSCHANGING: {
        WINDOWPOS *wp = (WINDOWPOS *)lParam;
        if (o && o->anchor_x >= 0) {
            wp->x = o->anchor_x;
            wp->y = o->anchor_y;
            wp->flags &= ~SWP_NOMOVE;
        }
        break;
    }
    }
    (void)wParam;
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

static bool make_dib(Overlay *o, int w, int h) {
    BITMAPINFO bmi;
    if (o->dib) {
        DeleteObject(o->dib);
        o->dib = NULL;
        o->bits = NULL;
    }
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    o->dib_w = w;
    o->dib_h = h;
    o->dib = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, &o->bits, NULL, 0);
    return o->dib && o->bits;
}

bool overlay_init(Overlay *o, int w, int h) {
    WNDCLASSA wc;
    memset(o, 0, sizeof(*o));
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = OVERLAY_CLASS;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassA(&wc);
    ensure_fonts();

    o->win_w = w;
    o->win_h = h;
    o->hwnd = CreateWindowExA(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TRANSPARENT,
        OVERLAY_CLASS, OVERLAY_TITLE,
        WS_POPUP | WS_VISIBLE,
        0, 0, w, h,
        NULL, NULL, wc.hInstance, NULL);
    if (!o->hwnd) return false;

    SetWindowLongPtrA(o->hwnd, GWLP_USERDATA, (LONG_PTR)o);

    load_layered();
    if (pSetLayeredWindowAttributes)
        pSetLayeredWindowAttributes(o->hwnd, COLKEY, 0, LWA_COLORKEY);

    overlay_anchor_topright(o);
    o->visible = 1;
    RegisterHotKey(o->hwnd, HOTKEY_QUIT_F12, MOD_NOREPEAT, VK_F12);
    RegisterHotKey(o->hwnd, HOTKEY_QUIT_END, MOD_NOREPEAT, VK_END);
    ShowWindow(o->hwnd, SW_SHOW);
    return make_dib(o, w, h);
}

void overlay_set_visible(Overlay *o, bool show) {
    if (!o || !o->hwnd) return;
    if (show) {
        if (!o->visible) {
            o->visible = 1;
            ShowWindow(o->hwnd, SW_SHOWNA);
        }
    } else if (o->visible) {
        o->visible = 0;
        ShowWindow(o->hwnd, SW_HIDE);
    }
}

void overlay_shutdown(Overlay *o) {
    if (o->hwnd) {
        UnregisterHotKey(o->hwnd, HOTKEY_QUIT_F12);
        UnregisterHotKey(o->hwnd, HOTKEY_QUIT_END);
    }
    if (o->dib) DeleteObject(o->dib);
    if (o->hwnd) DestroyWindow(o->hwnd);
    if (g_font_poi) { DeleteObject(g_font_poi); g_font_poi = NULL; }
    memset(o, 0, sizeof(*o));
}

void overlay_pump(void) {
    MSG msg;
    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            g_quit_requested = 1;
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

int overlay_want_quit(void) {
    return g_quit_requested;
}

static uint32_t px(uint8_t r, uint8_t g, uint8_t b) {
    return (uint32_t)b | ((uint32_t)g << 8) | ((uint32_t)r << 16);
}

static uint32_t px_op(uint8_t r, uint8_t g, uint8_t b, float op) {
    if (op <= 0.0f) return 0;
    if (op >= 1.0f) return px(r, g, b);
    return px((uint8_t)(r * op + 0.5f), (uint8_t)(g * op + 0.5f), (uint8_t)(b * op + 0.5f));
}

static void put(uint32_t *dst, int w, int h, int x, int y, uint32_t c) {
    if (x >= 0 && y >= 0 && x < w && y < h)
        dst[y * w + x] = c;
}

static void map_to_screen(float tile_x, float tile_y, float player_lx, float player_ly,
                          float scale, int cx, int cy, int *ox, int *oy) {
    float dx = tile_x - player_lx;
    float dy = tile_y - player_ly;
    float rx = dx * ISO_C - dy * ISO_C;
    float ry = dx * ISO_C + dy * ISO_C;
    *ox = cx + (int)(rx * scale);
    *oy = cy + (int)(ry * scale * 0.5f);
}

static void disc(uint32_t *dst, int w, int h, int cx, int cy, int rad, uint32_t c) {
    int y, x;
    for (y = -rad; y <= rad; y++)
        for (x = -rad; x <= rad; x++)
            if (x * x + y * y <= rad * rad)
                put(dst, w, h, cx + x, cy + y, c);
}

static void diamond(uint32_t *dst, int w, int h, int cx, int cy, int rad, uint32_t c) {
    int y, x;
    for (y = -rad; y <= rad; y++)
        for (x = -rad; x <= rad; x++)
            if (abs(x) + abs(y) <= rad)
                put(dst, w, h, cx + x, cy + y, c);
    put(dst, w, h, cx, cy, px(255, 255, 255));
}

static void fill_tile(uint32_t *dst, int w, int h, int sx, int sy, int tw, int th, uint32_t c) {
    int y, x;
    for (y = 0; y < th; y++)
        for (x = 0; x < tw; x++)
            put(dst, w, h, sx + x, sy + y, c);
}

static void line_pts(uint32_t *dst, int w, int h, int x0, int y0, int x1, int y1, uint32_t c) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;
    for (;;) {
        put(dst, w, h, x0, y0, c);
        put(dst, w, h, x0 + 1, y0, c);
        put(dst, w, h, x0, y0 + 1, c);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

#define COL_BOSS_VIOLET px(156, 39, 245) /* #9C27F5 */
#define COL_ELITE_WHITE px(255, 255, 255)
#define COL_NORMAL_GRAY px(110, 110, 120)
#define COL_MINION_GRAY px(165, 165, 175)

static int poi_on_screen(int sx, int sy, int w, int h, int margin) {
    return sx >= -margin && sy >= -margin && sx <= w + margin && sy <= h + margin;
}

static void clip_poi_to_edge(int cx, int cy, int tx, int ty, int w, int h, int pad, int *ox, int *oy) {
    int dx = tx - cx, dy = ty - cy;
    int lo = 0, hi = 1000, mid;
    if (dx == 0 && dy == 0) {
        *ox = cx;
        *oy = cy;
        return;
    }
    while (hi - lo > 1) {
        int px, py;
        mid = (lo + hi) / 2;
        px = cx + (dx * mid) / 1000;
        py = cy + (dy * mid) / 1000;
        if (px >= pad && px <= w - pad && py >= pad && py <= h - pad)
            lo = mid;
        else
            hi = mid;
    }
    mid = lo;
    *ox = cx + (dx * mid) / 1000;
    *oy = cy + (dy * mid) / 1000;
}

static void draw_immunity_ring(uint32_t *dst, int w, int h, int cx, int cy, int inner, int outer, int imm) {
    static const short stab[360] = {
        0,17,34,52,69,87,104,121,139,156,173,190,207,224,241,258,275,292,309,325,342,358,374,390,406,422,438,453,469,484,499,515,529,544,559,573,587,601,615,629,642,656,669,681,694,707,719,731,743,754,766,777,788,798,809,819,829,838,848,857,866,874,882,891,898,906,913,920,927,933,939,945,951,956,961,965,970,974,978,981,984,987,990,992,994,996,997,998,999,999,1000,999,999,998,997,996,994,992,990,987,984,981,978,974,970,965,961,956,951,945,939,933,927,920,913,906,898,891,882,874,866,857,848,838,829,819,809,798,788,777,766,754,743,731,719,707,694,681,669,656,642,629,615,601,587,573,559,544,529,515,499,484,469,453,438,422,406,390,374,358,342,325,309,292,275,258,241,224,207,190,173,156,139,121,104,87,69,52,34,17,0,-17,-34,-52,-69,-87,-104,-121,-139,-156,-173,-190,-207,-224,-241,-258,-275,-292,-309,-325,-342,-358,-374,-390,-406,-422,-438,-453,-469,-484,-500,-515,-529,-544,-559,-573,-587,-601,-615,-629,-642,-656,-669,-681,-694,-707,-719,-731,-743,-754,-766,-777,-788,-798,-809,-819,-829,-838,-848,-857,-866,-874,-882,-891,-898,-906,-913,-920,-927,-933,-939,-945,-951,-956,-961,-965,-970,-974,-978,-981,-984,-987,-990,-992,-994,-996,-997,-998,-999,-999,-1000,-999,-999,-998,-997,-996,-994,-992,-990,-987,-984,-981,-978,-974,-970,-965,-961,-956,-951,-945,-939,-933,-927,-920,-913,-906,-898,-891,-882,-874,-866,-857,-848,-838,-829,-819,-809,-798,-788,-777,-766,-754,-743,-731,-719,-707,-694,-681,-669,-656,-642,-629,-615,-601,-587,-573,-559,-544,-529,-515,-500,-484,-469,-453,-438,-422,-406,-390,-374,-358,-342,-325,-309,-292,-275,-258,-241,-224,-207,-190,-173,-156,-139,-121,-104,-87,-69,-52,-34,-17
    };
    uint32_t cols[6];
    int n = 0, deg, seg, r;
    if (imm & ELITE_IMM_FIRE)    cols[n++] = px(255, 0, 0);
    if (imm & ELITE_IMM_LIGHT)   cols[n++] = px(224, 224, 0);
    if (imm & ELITE_IMM_COLD)    cols[n++] = px(0, 0, 255);
    if (imm & ELITE_IMM_POISON)  cols[n++] = px(50, 205, 50);
    if (imm & ELITE_IMM_PHYS)    cols[n++] = px(205, 133, 63);
    if (imm & ELITE_IMM_MAGIC)   cols[n++] = px(255, 136, 0);
    if (n <= 0) return;

    for (deg = 0; deg < 360; deg++) {
        int s = stab[deg];
        int c = stab[(deg + 270) % 360];
        int x, y;
        seg = (deg * n) / 360;
        if (seg >= n) seg = n - 1;
        for (r = inner; r <= outer; r++) {
            x = cx + (c * r) / 1000;
            y = cy + (s * r) / 1000;
            put(dst, w, h, x, y, cols[seg]);
        }
    }
}

static void draw_player_cross(uint32_t *dst, int w, int h, int cx, int cy, uint32_t col) {
    int i, arm = 8;
    for (i = -arm; i <= arm; i++) {
        if (abs(i) < 2) continue;
        put(dst, w, h, cx + i, cy, col);
        put(dst, w, h, cx, cy + i, col);
    }
    disc(dst, w, h, cx, cy, 2, px(255, 255, 255));
}

static void draw_corpse_marker(uint32_t *dst, int w, int h, int cx, int cy) {
    uint32_t col = px(255, 0, 255);
    draw_player_cross(dst, w, h, cx, cy, col);
    disc(dst, w, h, cx, cy, 5, col);
}

static void draw_monster_marker(uint32_t *dst, int w, int h, int sx, int sy, MonKind kind, int imm) {
    int rad;
    uint32_t fill;

    switch (kind) {
    case MON_BOSS:
        rad = 7;
        fill = COL_BOSS_VIOLET;
        break;
    case MON_CHAMPION:
    case MON_UNIQUE:
    case MON_SUPERUNIQUE:
        rad = 4;
        fill = COL_ELITE_WHITE;
        break;
    case MON_MINION:
        rad = 2;
        fill = COL_MINION_GRAY;
        break;
    default:
        rad = 2;
        fill = COL_NORMAL_GRAY;
        break;
    }

    if (imm)
        draw_immunity_ring(dst, w, h, sx, sy, rad + 1, rad + 2, imm);
    disc(dst, w, h, sx, sy, rad, fill);
}

static int monster_layer(MonKind kind) {
    if (kind == MON_BOSS) return 2;
    if (kind == MON_CHAMPION || kind == MON_UNIQUE || kind == MON_SUPERUNIQUE) return 1;
    return 0;
}

static int dest_on_map(const LevelMap *map, float lx, float ly) {
    return lx >= 0.0f && ly >= 0.0f && lx < (float)map->w && ly < (float)map->h;
}

/* Canyon (46) : ignorer les faux tombeaux 66-72, garder isGoodExit. */
static int is_fake_tomb_exit(uint32_t level_id, const MapPoi *poi) {
    if ((int)level_id != 46) return 0;
    if (!poi || (poi->kind != POI_EXIT && poi->kind != POI_GOOD_EXIT)) return 0;
    if (poi->id < 66 || poi->id > 72) return 0;
    return poi->kind != POI_GOOD_EXIT;
}

static int guide_hidden_by_boss(const RemoteGuide *g, const RemoteBoss *bosses, int n) {
    int i;
    if (!g || !bosses) return 0;
    for (i = 0; i < n; i++) {
        if (bosses[i].boss_level != g->dest_level) continue;
        if (boss_state_show_spawn(bosses[i].boss_level))
            return 1;
    }
    return 0;
}

static int find_exit_to(const LevelMap *map, int dest_level, int *ox, int *oy) {
    int i, found = 0;
    int best_x = 0, best_y = 0;
    int best_good = 0;
    if (dest_level <= 0) return 0;
    for (i = 0; i < map->poi_count; i++) {
        const MapPoi *p = &map->pois[i];
        if (p->kind != POI_EXIT && p->kind != POI_GOOD_EXIT) continue;
        if (p->id != dest_level) continue;
        if (found && best_good && p->kind != POI_GOOD_EXIT) continue;
        best_x = p->x;
        best_y = p->y;
        best_good = (p->kind == POI_GOOD_EXIT);
        found = 1;
        if (best_good) break;
    }
    if (!found) return 0;
    *ox = best_x;
    *oy = best_y;
    return 1;
}

/* Ouverture walkable au bord de la carte, la plus proche de la cible (sortie de zone). */
static int nearest_map_sortie(const LevelMap *map, float dest_lx, float dest_ly, int *ox, int *oy) {
    int x, y, found = 0;
    float best = 1e12f;
    int margin = 8;
    for (y = 0; y < map->h; y++) {
        for (x = 0; x < map->w; x++) {
            float dx, dy, d;
            if (x >= margin && y >= margin && x < map->w - margin && y < map->h - margin)
                continue;
            if (!map->walk[y][x]) continue;
            dx = (float)x - dest_lx;
            dy = (float)y - dest_ly;
            d = dx * dx + dy * dy;
            if (!found || d < best) {
                best = d;
                *ox = x;
                *oy = y;
                found = 1;
            }
        }
    }
    return found;
}

static void resolve_line_dest(const LevelMap *map, float dest_lx, float dest_ly,
                              int dest_level, float *ox, float *oy) {
    int x, y;
    if (find_exit_to(map, dest_level, &x, &y)) {
        *ox = (float)x;
        *oy = (float)y;
        return;
    }
    if (dest_on_map(map, dest_lx, dest_ly)) {
        *ox = dest_lx;
        *oy = dest_ly;
        return;
    }
    if (nearest_map_sortie(map, dest_lx, dest_ly, &x, &y)) {
        *ox = (float)x;
        *oy = (float)y;
        return;
    }
    *ox = dest_lx;
    *oy = dest_ly;
}

static void text_outline(HDC hdc, int x, int y, const char *s, COLORREF col) {
    SetTextColor(hdc, RGB(20, 20, 24));
    TextOutA(hdc, x + 1, y + 1, s, (int)strlen(s));
    TextOutA(hdc, x - 1, y, s, (int)strlen(s));
    TextOutA(hdc, x + 1, y, s, (int)strlen(s));
    TextOutA(hdc, x, y - 1, s, (int)strlen(s));
    TextOutA(hdc, x, y + 1, s, (int)strlen(s));
    SetTextColor(hdc, col);
    TextOutA(hdc, x, y, s, (int)strlen(s));
}

void overlay_render(Overlay *o, const LevelMap *map, float player_world_x, float player_world_y,
                    uint32_t seed, uint32_t diff, uint32_t level_id, const char *name,
                    const MonList *monsters, bool player_is_corpse, const Settings *settings,
                    const RemoteBoss *remote_bosses, int remote_boss_count,
                    const RemoteGuide *remote_guides, int remote_guide_count) {
    uint32_t *dst;
    int x, y, i;
    HDC hdc, mem;
    HGDIOBJ old;
    float local_x, local_y;
    float scale = MAP_SCALE;
    int cx, cy;
    int tile_w, tile_h;
    int layer;
    uint32_t col_exit = px(255, 70, 160);
    uint32_t col_exit_line = px(255, 90, 175);
    uint32_t col_wp = px(255, 210, 40);
    uint32_t col_wp_line = px(255, 200, 50);
    uint32_t col_portal = px(200, 120, 255);
    uint32_t col_boss = px(156, 39, 245);
    uint32_t col_boss_line = px(180, 80, 255);
    uint32_t col_quest = px(80, 255, 120);
    uint32_t col_quest_line = px(60, 220, 100);
    float map_op = settings ? settings->map_opacity : 0.85f;
    uint32_t col_edge;

    if (!o || !o->bits || !map || !map->ready) return;
    if (map_op < 0.05f) map_op = 0.05f;
    if (map_op > 1.0f) map_op = 1.0f;
    col_edge = px_op(170, 170, 170, map_op);

    overlay_anchor_topright(o);
    ensure_fonts();
    dst = (uint32_t *)o->bits;

    local_x = player_world_x - (float)map->offset_x;
    local_y = player_world_y - (float)map->offset_y;

    cx = o->dib_w / 2;
    cy = o->dib_h / 2;
    tile_w = (int)(scale + 0.5f);
    if (tile_w < 2) tile_w = 2;
    tile_h = (int)(scale * 0.5f + 0.5f);
    if (tile_h < 1) tile_h = 1;

    memset(dst, 0, (size_t)o->dib_w * (size_t)o->dib_h * 4);

    for (y = 0; y < map->h; y++) {
        for (x = 0; x < map->w; x++) {
            int sx, sy;
            if (!map->edge[y][x]) continue;
            map_to_screen((float)x + 0.5f, (float)y + 0.5f, local_x, local_y, scale, cx, cy, &sx, &sy);
            if (sx < -4 || sy < -4 || sx > o->dib_w + 4 || sy > o->dib_h + 4) continue;
            fill_tile(dst, o->dib_w, o->dib_h, sx - tile_w / 2, sy - tile_h / 2, tile_w, tile_h, col_edge);
        }
    }

    {
        int paths_done = 0;
        int x0, y0, x1, y1;
        map_to_screen(local_x, local_y, local_x, local_y, scale, cx, cy, &x0, &y0);
        for (i = 0; i < map->poi_count; i++) {
            if (map->pois[i].kind == POI_WAYPOINT) {
                map_to_screen((float)map->pois[i].x, (float)map->pois[i].y,
                              local_x, local_y, scale, cx, cy, &x1, &y1);
                line_pts(dst, o->dib_w, o->dib_h, x0, y0, x1, y1, col_wp_line);
                break;
            }
        }
        for (i = 0; i < remote_guide_count && remote_guides; i++) {
            float glx, gly, tx, ty;
            uint32_t line_col;
            if (guide_hidden_by_boss(&remote_guides[i], remote_bosses, remote_boss_count))
                continue;
            glx = remote_guides[i].world_x - (float)map->offset_x;
            gly = remote_guides[i].world_y - (float)map->offset_y;
            line_col = remote_guides[i].is_waypoint ? col_wp_line : col_quest_line;
            resolve_line_dest(map, glx, gly, remote_guides[i].dest_level, &tx, &ty);
            map_to_screen(tx, ty, local_x, local_y, scale, cx, cy, &x1, &y1);
            line_pts(dst, o->dib_w, o->dib_h, x0, y0, x1, y1, line_col);
        }
        for (i = 0; i < map->poi_count; i++) {
            const MapPoi *poi = &map->pois[i];
            if (poi->kind == POI_BOSS_SPAWN) {
                if (!boss_state_show_spawn((int)level_id)) continue;
                map_to_screen((float)poi->x, (float)poi->y, local_x, local_y, scale, cx, cy, &x1, &y1);
                line_pts(dst, o->dib_w, o->dib_h, x0, y0, x1, y1, col_boss_line);
            }
        }
        for (i = 0; i < remote_boss_count && remote_bosses; i++) {
            if (!boss_state_show_spawn(remote_bosses[i].boss_level)) continue;
            float blx = remote_bosses[i].world_x - (float)map->offset_x;
            float bly = remote_bosses[i].world_y - (float)map->offset_y;
            float tx, ty;
            resolve_line_dest(map, blx, bly, remote_bosses[i].boss_level, &tx, &ty);
            map_to_screen(tx, ty, local_x, local_y, scale, cx, cy, &x1, &y1);
            line_pts(dst, o->dib_w, o->dib_h, x0, y0, x1, y1, col_boss_line);
        }
        for (i = 0; i < map->poi_count; i++) {
            const MapPoi *poi = &map->pois[i];
            if (poi->kind == POI_QUEST) {
                if (!boss_state_show_quest((int)level_id, poi->label)) continue;
                map_to_screen((float)poi->x, (float)poi->y, local_x, local_y, scale, cx, cy, &x1, &y1);
                line_pts(dst, o->dib_w, o->dib_h, x0, y0, x1, y1, col_quest_line);
            }
        }
        for (i = 0; i < map->poi_count && paths_done < 2; i++) {
            const MapPoi *poi = &map->pois[i];
            if (poi->kind != POI_EXIT && poi->kind != POI_GOOD_EXIT)
                continue;
            if (is_fake_tomb_exit(level_id, poi))
                continue;
            map_to_screen((float)poi->x, (float)poi->y, local_x, local_y, scale, cx, cy, &x1, &y1);
            line_pts(dst, o->dib_w, o->dib_h, x0, y0, x1, y1, col_exit_line);
            paths_done++;
        }
    }

    for (i = 0; i < map->poi_count; i++) {
        const MapPoi *poi = &map->pois[i];
        int sx, sy, mx, my, off;
        if (is_fake_tomb_exit(level_id, poi))
            continue;
        map_to_screen((float)poi->x, (float)poi->y, local_x, local_y, scale, cx, cy, &sx, &sy);
        mx = sx;
        my = sy;
        off = !poi_on_screen(sx, sy, o->dib_w, o->dib_h, 20);
        if (off)
            clip_poi_to_edge(cx, cy, sx, sy, o->dib_w, o->dib_h, 14, &mx, &my);
        else if (sx < -20 || sy < -20 || sx > o->dib_w + 20 || sy > o->dib_h + 20)
            continue;

        if (poi->kind == POI_WAYPOINT) {
            disc(dst, o->dib_w, o->dib_h, mx, my, off ? 5 : 7, col_wp);
            if (off)
                disc(dst, o->dib_w, o->dib_h, mx, my, 2, px(255, 255, 255));
        } else if (poi->kind == POI_PORTAL)
            diamond(dst, o->dib_w, o->dib_h, mx, my, off ? 5 : 7, col_portal);
        else if (poi->kind == POI_BOSS_SPAWN) {
            if (!boss_state_show_spawn((int)level_id)) continue;
            int r = off ? 5 : 7;
            disc(dst, o->dib_w, o->dib_h, mx, my, r, col_boss);
            disc(dst, o->dib_w, o->dib_h, mx, my, r - 2, px(255, 255, 255));
            disc(dst, o->dib_w, o->dib_h, mx, my, r - 4, col_boss);
        } else if (poi->kind == POI_QUEST) {
            if (!boss_state_show_quest((int)level_id, poi->label)) continue;
            diamond(dst, o->dib_w, o->dib_h, mx, my, off ? 5 : 6, col_quest);
        } else if (poi->kind == POI_EXIT || poi->kind == POI_GOOD_EXIT)
            diamond(dst, o->dib_w, o->dib_h, mx, my, off ? 5 : 6, col_exit);
    }

    for (i = 0; i < remote_boss_count && remote_bosses; i++) {
        float blx = remote_bosses[i].world_x - (float)map->offset_x;
        float bly = remote_bosses[i].world_y - (float)map->offset_y;
        float tx, ty;
        int sx, sy, mx, my, off, r;
        if (!boss_state_show_spawn(remote_bosses[i].boss_level)) continue;
        resolve_line_dest(map, blx, bly, remote_bosses[i].boss_level, &tx, &ty);
        map_to_screen(tx, ty, local_x, local_y, scale, cx, cy, &sx, &sy);
        mx = sx;
        my = sy;
        off = !poi_on_screen(sx, sy, o->dib_w, o->dib_h, 20);
        if (off)
            clip_poi_to_edge(cx, cy, sx, sy, o->dib_w, o->dib_h, 14, &mx, &my);
        else if (sx < -20 || sy < -20 || sx > o->dib_w + 20 || sy > o->dib_h + 20)
            continue;
        r = off ? 5 : 7;
        disc(dst, o->dib_w, o->dib_h, mx, my, r, col_boss);
        disc(dst, o->dib_w, o->dib_h, mx, my, r - 2, px(255, 255, 255));
        disc(dst, o->dib_w, o->dib_h, mx, my, r - 4, col_boss);
    }

    for (i = 0; i < remote_guide_count && remote_guides; i++) {
        float glx, gly, tx, ty;
        int sx, sy, mx, my, off;
        if (guide_hidden_by_boss(&remote_guides[i], remote_bosses, remote_boss_count))
            continue;
        glx = remote_guides[i].world_x - (float)map->offset_x;
        gly = remote_guides[i].world_y - (float)map->offset_y;
        resolve_line_dest(map, glx, gly, remote_guides[i].dest_level, &tx, &ty);
        map_to_screen(tx, ty, local_x, local_y, scale, cx, cy, &sx, &sy);
        mx = sx;
        my = sy;
        off = !poi_on_screen(sx, sy, o->dib_w, o->dib_h, 20);
        if (off)
            clip_poi_to_edge(cx, cy, sx, sy, o->dib_w, o->dib_h, 14, &mx, &my);
        else if (sx < -20 || sy < -20 || sx > o->dib_w + 20 || sy > o->dib_h + 20)
            continue;
        if (remote_guides[i].is_waypoint) {
            disc(dst, o->dib_w, o->dib_h, mx, my, off ? 5 : 7, col_wp);
            if (off)
                disc(dst, o->dib_w, o->dib_h, mx, my, 2, px(255, 255, 255));
        } else {
            diamond(dst, o->dib_w, o->dib_h, mx, my, off ? 5 : 6, col_quest);
        }
    }

    if (monsters) {
        for (layer = 0; layer <= 2; layer++) {
            for (i = 0; i < monsters->count; i++) {
                const MonUnit *m = &monsters->list[i];
                float mx = m->x - (float)map->offset_x;
                float my = m->y - (float)map->offset_y;
                int sx, sy;
                if (monster_layer(m->kind) != layer) continue;
                map_to_screen(mx, my, local_x, local_y, scale, cx, cy, &sx, &sy);
                if (sx < -24 || sy < -24 || sx > o->dib_w + 24 || sy > o->dib_h + 24) continue;
                draw_monster_marker(dst, o->dib_w, o->dib_h, sx, sy, m->kind, m->immunities);
            }
        }
    }

    if (player_is_corpse)
        draw_corpse_marker(dst, o->dib_w, o->dib_h, cx, cy);
    else {
        disc(dst, o->dib_w, o->dib_h, cx, cy, 6, px(255, 50, 50));
        disc(dst, o->dib_w, o->dib_h, cx, cy, 2, px(255, 255, 255));
        for (i = -10; i <= 10; i++) {
            if (abs(i) < 3) continue;
            put(dst, o->dib_w, o->dib_h, cx + i, cy, px(255, 220, 140));
            put(dst, o->dib_w, o->dib_h, cx, cy + i, px(255, 220, 140));
        }
    }

    hdc = GetDC(o->hwnd);
    mem = CreateCompatibleDC(hdc);
    old = SelectObject(mem, o->dib);
    BitBlt(hdc, 0, 0, o->dib_w, o->dib_h, mem, 0, 0, SRCCOPY);

    SetBkMode(hdc, TRANSPARENT);
    SelectObject(hdc, g_font_poi);
    for (i = 0; i < map->poi_count; i++) {
        const MapPoi *poi = &map->pois[i];
        SIZE sz;
        int sx, sy, lx, ly;
        COLORREF col;
        if (poi->kind == POI_QUEST && !boss_state_show_quest((int)level_id, poi->label))
            continue;
        if (poi->kind != POI_EXIT && poi->kind != POI_GOOD_EXIT &&
            poi->kind != POI_WAYPOINT && poi->kind != POI_PORTAL &&
            poi->kind != POI_BOSS_SPAWN && poi->kind != POI_QUEST)
            continue;
        if (poi->kind == POI_BOSS_SPAWN && !boss_state_show_spawn((int)level_id))
            continue;
        if (is_fake_tomb_exit(level_id, poi))
            continue;
        map_to_screen((float)poi->x, (float)poi->y, local_x, local_y, scale, cx, cy, &sx, &sy);
        lx = sx;
        ly = sy;
        if (!poi_on_screen(sx, sy, o->dib_w, o->dib_h, 20))
            clip_poi_to_edge(cx, cy, sx, sy, o->dib_w, o->dib_h, 18, &lx, &ly);
        else if (sx < 0 || sy < 0 || sx > o->dib_w || sy > o->dib_h)
            continue;
        ly -= 14;
        if (lx < 0 || ly < 0 || lx > o->dib_w || ly > o->dib_h) continue;
        GetTextExtentPoint32A(hdc, poi->label, (int)strlen(poi->label), &sz);
        lx -= sz.cx / 2;
        if (poi->kind == POI_WAYPOINT) col = RGB(255, 220, 60);
        else if (poi->kind == POI_PORTAL) col = RGB(210, 150, 255);
        else if (poi->kind == POI_BOSS_SPAWN) col = RGB(180, 80, 255);
        else if (poi->kind == POI_QUEST) col = RGB(100, 255, 140);
        else col = RGB(255, 110, 180);
        text_outline(hdc, lx, ly, poi->label, col);
    }
    for (i = 0; i < remote_boss_count && remote_bosses; i++) {
        SIZE sz;
        int sx, sy, lx, ly;
        float blx = remote_bosses[i].world_x - (float)map->offset_x;
        float bly = remote_bosses[i].world_y - (float)map->offset_y;
        float tx, ty;
        if (!boss_state_show_spawn(remote_bosses[i].boss_level)) continue;
        resolve_line_dest(map, blx, bly, remote_bosses[i].boss_level, &tx, &ty);
        map_to_screen(tx, ty, local_x, local_y, scale, cx, cy, &sx, &sy);
        lx = sx;
        ly = sy;
        if (!poi_on_screen(sx, sy, o->dib_w, o->dib_h, 20))
            clip_poi_to_edge(cx, cy, sx, sy, o->dib_w, o->dib_h, 18, &lx, &ly);
        else if (sx < 0 || sy < 0 || sx > o->dib_w || sy > o->dib_h)
            continue;
        ly -= 14;
        if (lx < 0 || ly < 0 || lx > o->dib_w || ly > o->dib_h) continue;
        GetTextExtentPoint32A(hdc, remote_bosses[i].label, (int)strlen(remote_bosses[i].label), &sz);
        lx -= sz.cx / 2;
        text_outline(hdc, lx, ly, remote_bosses[i].label, RGB(180, 80, 255));
    }
    for (i = 0; i < remote_guide_count && remote_guides; i++) {
        SIZE sz;
        int sx, sy, lx, ly;
        float glx, gly, tx, ty;
        if (guide_hidden_by_boss(&remote_guides[i], remote_bosses, remote_boss_count))
            continue;
        glx = remote_guides[i].world_x - (float)map->offset_x;
        gly = remote_guides[i].world_y - (float)map->offset_y;
        resolve_line_dest(map, glx, gly, remote_guides[i].dest_level, &tx, &ty);
        map_to_screen(tx, ty, local_x, local_y, scale, cx, cy, &sx, &sy);
        lx = sx;
        ly = sy;
        if (!poi_on_screen(sx, sy, o->dib_w, o->dib_h, 20))
            clip_poi_to_edge(cx, cy, sx, sy, o->dib_w, o->dib_h, 18, &lx, &ly);
        else if (sx < 0 || sy < 0 || sx > o->dib_w || sy > o->dib_h)
            continue;
        ly -= 14;
        if (lx < 0 || ly < 0 || lx > o->dib_w || ly > o->dib_h) continue;
        GetTextExtentPoint32A(hdc, remote_guides[i].label, (int)strlen(remote_guides[i].label), &sz);
        lx -= sz.cx / 2;
        text_outline(hdc, lx, ly, remote_guides[i].label,
                     remote_guides[i].is_waypoint ? RGB(255, 220, 60) : RGB(100, 255, 140));
    }
    SelectObject(mem, old);
    DeleteDC(mem);
    ReleaseDC(o->hwnd, hdc);
    (void)seed;
    (void)diff;
}
