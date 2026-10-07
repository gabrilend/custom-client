/*
 * 018-maps.c (issue 173k) - see 018-maps.h. Ported line for line from
 * src/custom-client/lib/maps.lua; the tests hold both to the same numbers.
 */
#include "018-maps.h"

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* height block flags (GridTerrainData.h) */
#define HEIGHT_NONE  0x0001
#define HEIGHT_INT16 0x0002
#define HEIGHT_INT8  0x0004

/* holes: a cell of 8 x 8 squares carries 16 bits, one per 2 x 2 squares */
static const uint16_t HOLE_H[4] = { 0x1111, 0x2222, 0x4444, 0x8888 };
static const uint16_t HOLE_V[4] = { 0x000F, 0x00F0, 0x0F00, 0xF000 };

/* {{{ static u32, f32 */
static uint32_t u32(const unsigned char *s, size_t at)
{
    return (uint32_t)s[at] | ((uint32_t)s[at + 1] << 8) | ((uint32_t)s[at + 2] << 16) | ((uint32_t)s[at + 3] << 24);
}
static float f32(const unsigned char *s, size_t at)
{
    uint32_t v = u32(s, at);
    float f;
    memcpy(&f, &v, 4);
    return f;
}
/* }}} */

/* {{{ maps_tile_of */
void maps_tile_of(float x, float y, int *tx, int *ty)
{
    *tx = (int)floorf(32.0f - x / MAPS_GRID_SIZE);
    *ty = (int)floorf(32.0f - y / MAPS_GRID_SIZE);
}
/* }}} */

/* {{{ maps_load_tile
 * Reads one tile, or NULL with the reason in `why` when the file is
 * missing or not a map file. Heights stored as whole numbers (int16 /
 * int8) become yards here (raw x step + base): the server's triangle sums
 * are linear, so decoding first changes nothing but rounding. */
MapTile *maps_load_tile(const char *dir, int map_id, int tx, int ty, char *why, int why_len)
{
    char path[640];
    snprintf(path, sizeof path, "%s/%03d%02d%02d.map", dir, map_id, tx, ty);
    FILE *f = fopen(path, "rb");
    if (!f) { snprintf(why, why_len, "no tile file %s", path); return NULL; }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *s = malloc((size_t)len);
    if (fread(s, 1, (size_t)len, f) != (size_t)len) { fclose(f); free(s); snprintf(why, why_len, "%s: short read", path); return NULL; }
    fclose(f);
    if (len < 44 || memcmp(s, "MAPS", 4) != 0) { free(s); snprintf(why, why_len, "%s is not a map file (no MAPS mark)", path); return NULL; }

    size_t height_at = u32(s, 20);                       /* heightMapOffset (6th field) */
    size_t holes_at = u32(s, 36), holes_size = u32(s, 40);
    if (memcmp(s + height_at, "MHGT", 4) != 0) { free(s); snprintf(why, why_len, "%s: height block missing", path); return NULL; }

    MapTile *t = calloc(1, sizeof *t);
    t->tx = tx; t->ty = ty;
    uint32_t flags = u32(s, height_at + 4);
    float base = f32(s, height_at + 8), top = f32(s, height_at + 12);
    size_t at = height_at + 16;
    if (flags & HEIGHT_NONE) {
        t->is_flat = 1;
        t->flat = base;
    } else {
        t->v9 = malloc(sizeof(float) * 129 * 129);
        t->v8 = malloc(sizeof(float) * 128 * 128);
        if (flags & HEIGHT_INT16) {
            float step = (top - base) / 65535.0f;
            for (int i = 0; i < 129 * 129; ++i) t->v9[i] = (float)(s[at + i * 2] | (s[at + i * 2 + 1] << 8)) * step + base;
            size_t c = at + 129 * 129 * 2;
            for (int i = 0; i < 128 * 128; ++i) t->v8[i] = (float)(s[c + i * 2] | (s[c + i * 2 + 1] << 8)) * step + base;
        } else if (flags & HEIGHT_INT8) {
            float step = (top - base) / 255.0f;
            for (int i = 0; i < 129 * 129; ++i) t->v9[i] = (float)s[at + i] * step + base;
            for (int i = 0; i < 128 * 128; ++i) t->v8[i] = (float)s[at + 129 * 129 + i] * step + base;
        } else {
            for (int i = 0; i < 129 * 129; ++i) t->v9[i] = f32(s, at + (size_t)i * 4);
            for (int i = 0; i < 128 * 128; ++i) t->v8[i] = f32(s, at + (size_t)(129 * 129 + i) * 4);
        }
    }
    if (holes_size > 0) {
        t->has_holes = 1;
        for (int i = 0; i < 256; ++i) t->holes[i] = (uint16_t)(s[holes_at + i * 2] | (s[holes_at + i * 2 + 1] << 8));
    }
    free(s);
    return t;
}
/* }}} */

/* {{{ maps_free_tile */
void maps_free_tile(MapTile *t)
{
    if (!t || t == MAPS_NO_TILE) return;
    free(t->v9);
    free(t->v8);
    free(t);
}
/* }}} */

/* {{{ maps_is_hole
 * The server's isHole: cell (row, col) of the tile's 128 x 128. */
int maps_is_hole(const MapTile *t, int row, int col)
{
    if (!t->has_holes) return 0;
    int cell_row = row / 8, cell_col = col / 8;
    int hole_row = (row % 8) / 2, hole_col = (col - cell_col * 8) / 2;
    uint16_t hole = t->holes[cell_row * 16 + cell_col];
    return (hole & HOLE_H[hole_col] & HOLE_V[hole_row]) != 0;
}
/* }}} */

/* {{{ maps_height_in_tile
 * The ground height at world (x, y) inside tile t, exactly as the server
 * works it out: which of the cell's four triangles the point is in, then
 * the plane through that triangle; and that plane's rise per yard along
 * world x and y (the exact steepness the mover holds to the slope rule).
 * MAPS_INVALID_HEIGHT in a hole. */
float maps_height_in_tile(const MapTile *t, float x, float y, float *gx, float *gy)
{
    if (t->is_flat) { *gx = 0; *gy = 0; return t->flat; }
    float fx = MAPS_RESOLUTION * (32.0f - x / MAPS_GRID_SIZE);
    float fy = MAPS_RESOLUTION * (32.0f - y / MAPS_GRID_SIZE);
    int xi = (int)floorf(fx), yi = (int)floorf(fy);
    fx -= (float)xi; fy -= (float)yi;
    xi &= MAPS_RESOLUTION - 1; yi &= MAPS_RESOLUTION - 1;
    if (maps_is_hole(t, xi, yi)) { *gx = 0; *gy = 0; return MAPS_INVALID_HEIGHT; }

    const float *v9 = t->v9;
    float h5 = 2.0f * t->v8[xi * 128 + yi];
    float a, b, c;
    if (fx + fy < 1.0f) {
        if (fx > fy) {                                   /* triangle 1: h1, h2, h5 */
            float h1 = v9[xi * 129 + yi], h2 = v9[(xi + 1) * 129 + yi];
            a = h2 - h1; b = h5 - h1 - h2; c = h1;
        } else {                                         /* triangle 2: h1, h3, h5 */
            float h1 = v9[xi * 129 + yi], h3 = v9[xi * 129 + yi + 1];
            a = h5 - h1 - h3; b = h3 - h1; c = h1;
        }
    } else {
        if (fx > fy) {                                   /* triangle 3: h2, h4, h5 */
            float h2 = v9[(xi + 1) * 129 + yi], h4 = v9[(xi + 1) * 129 + yi + 1];
            a = h2 + h4 - h5; b = h4 - h2; c = h5 - h4;
        } else {                                         /* triangle 4: h3, h4, h5 */
            float h3 = v9[xi * 129 + yi + 1], h4 = v9[(xi + 1) * 129 + yi + 1];
            a = h4 - h3; b = h3 + h4 - h5; c = h5 - h4;
        }
    }
    /* fx and fy fall as x and y rise (MAPS_RESOLUTION cells a tile) */
    *gx = -a * MAPS_RESOLUTION / MAPS_GRID_SIZE;
    *gy = -b * MAPS_RESOLUTION / MAPS_GRID_SIZE;
    return a * fx + b * fy + c;
}
/* }}} */

/* {{{ maps_corner_position
 * World position of corner (ix, iy) of tile (tx, ty); fractions give cell centres. */
void maps_corner_position(int tx, int ty, float ix, float iy, float *x, float *y)
{
    *x = (32.0f - (float)tx - ix / MAPS_RESOLUTION) * MAPS_GRID_SIZE;
    *y = (32.0f - (float)ty - iy / MAPS_RESOLUTION) * MAPS_GRID_SIZE;
}
/* }}} */

/* {{{ maps_world_* */
void maps_world_init(MapWorld *w, const char *dir, int map_id)
{
    memset(w, 0, sizeof *w);
    snprintf(w->dir, sizeof w->dir, "%s", dir);
    w->map_id = map_id;
}
/* Two threads ask (the main one, and the reading-ahead one preparing
 * ground: 173u). A tile once in its slot never changes or goes until
 * maps_world_free, so a filled slot is read without the lock; an empty
 * one is filled under it, checked again inside so it loads only once. */
static pthread_mutex_t world_lock = PTHREAD_MUTEX_INITIALIZER;
MapTile *maps_world_tile(MapWorld *w, int tx, int ty)
{
    if (tx < 0 || tx >= 64 || ty < 0 || ty >= 64) return NULL;
    MapTile **slot = &w->tiles[tx * 64 + ty];
    MapTile *t = __atomic_load_n(slot, __ATOMIC_ACQUIRE);
    if (!t) {
        pthread_mutex_lock(&world_lock);
        t = *slot;
        if (!t) {
            char why[700];
            t = maps_load_tile(w->dir, w->map_id, tx, ty, why, sizeof why);
            if (!t) t = MAPS_NO_TILE;                    /* none there: asked once, remembered */
            __atomic_store_n(slot, t, __ATOMIC_RELEASE);
        }
        pthread_mutex_unlock(&world_lock);
    }
    return t == MAPS_NO_TILE ? NULL : t;
}
float maps_world_height(MapWorld *w, float x, float y, float *gx, float *gy)
{
    int tx, ty;
    maps_tile_of(x, y, &tx, &ty);
    MapTile *t = maps_world_tile(w, tx, ty);
    if (!t) { *gx = 0; *gy = 0; return MAPS_INVALID_HEIGHT; }
    return maps_height_in_tile(t, x, y, gx, gy);
}
void maps_world_free(MapWorld *w)
{
    for (int i = 0; i < 64 * 64; ++i) maps_free_tile(w->tiles[i]);
}
/* }}} */
