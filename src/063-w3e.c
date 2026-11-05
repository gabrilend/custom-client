/*
 * 063-w3e.c (issue 302a) - see 063-w3e.h. Ported from
 * world-edit-to-execute's src/parsers/w3e.lua (the reading) and the
 * reading half of src/assets/ground.lua (ground.variation, ground.cells,
 * and the cells ground.lay leaves bare), keeping their arithmetic so both
 * give the same numbers: heights in doubles where the Lua's are, floats
 * where the Lua packs them into float arrays. Nothing is kept in this
 * file but constants.
 */
#include "063-w3e.h"
#include "013-errors.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* the main tileset's letter -> its name (the Lua's TILESETS) */
static const struct { char code; const char *name; } TILESETS[] = {
    { 'A', "ashenvale" }, { 'B', "barrens" }, { 'C', "felwood" }, { 'D', "dungeon" },
    { 'F', "lordaeron_fall" }, { 'G', "underground" }, { 'I', "icecrown" },
    { 'J', "dalaran_ruins" }, { 'K', "black_citadel" }, { 'L', "lordaeron_summer" },
    { 'N', "northrend" }, { 'O', "outland" }, { 'Q', "village_fall" }, { 'V', "village" },
    { 'W', "lordaeron_winter" }, { 'X', "dalaran" }, { 'Y', "cityscape" }, { 'Z', "sunken_ruins" },
};

/* corner bits of a cell's higher layers (the Lua's ground.BITS) */
enum { BIT_SE = 1, BIT_SW = 2, BIT_NE = 4, BIT_NW = 8 };

/* {{{ static rd32, rd16, rdf: little-endian reads; the caller checks the bounds */
static int32_t rd32(const unsigned char *p)
{
    return (int32_t)((uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24);
}
static uint16_t rd16(const unsigned char *p) { return (uint16_t)(p[0] | p[1] << 8); }
static float rdf(const unsigned char *p)
{
    uint32_t v = (uint32_t)rd32(p);
    float f;
    memcpy(&f, &v, 4);
    return f;
}
/* }}} */

/* {{{ static ids: a tileset count and its ids, at *pos */
static WErr ids(const unsigned char *s, size_t len, size_t *pos, const char *kind, int *n, char **out, WWhy *why)
{
    if (len - *pos < 4) return w_fail(why, W_ERR_CORRUPT, "war3map.w3e ends before its %s tileset count (at byte %zu of %zu)", kind, *pos, len);
    int32_t c = rd32(s + *pos);
    *pos += 4;
    if (c < 0) return w_fail(why, W_ERR_CORRUPT, "war3map.w3e's %s tileset count is %d, below 0 (at byte %zu)", kind, (int)c, *pos - 4);
    if ((size_t)c > (len - *pos) / 4)
        return w_fail(why, W_ERR_CORRUPT, "war3map.w3e lists %d %s tilesets but has only %zu bytes left (at byte %zu)", (int)c, kind, len - *pos, *pos);
    *out = malloc(c ? (size_t)c * 4 : 1);
    if (!*out) return w_fail(why, W_ERR_FULL, "no memory for war3map.w3e's %d %s tileset ids", (int)c, kind);
    memcpy(*out, s + *pos, (size_t)c * 4);
    *pos += (size_t)c * 4;
    *n = (int)c;
    return W_OK;
}
/* }}} */

/* {{{ static point: one 7-byte tile point (the Lua's parse_tilepoint) */
static void point(const unsigned char *b, W3ePoint *tp)
{
    tp->height_raw = (int16_t)rd16(b);
    tp->height = (float)((tp->height_raw - 8192) / 4.0);
    uint16_t w = rd16(b + 2);
    tp->water_raw = w & 0x3FFF;
    tp->water_level = (float)((tp->water_raw - 8192) / 4.0);
    tp->water_flags = w & 0xC000;
    tp->boundary = tp->water_flags != 0;
    tp->ground_texture = b[4] & 0x0F;
    tp->is_ramp = (b[4] & 0x10) != 0;
    tp->is_blight = (b[4] & 0x20) != 0;
    tp->has_water = (b[4] & 0x40) != 0;
    tp->is_boundary = (b[4] & 0x80) != 0;
    tp->texture_details = b[5] & 0x1F;
    tp->cliff_variation = (b[5] >> 5) & 0x07;
    tp->layer_height = b[6] & 0x0F;
    tp->cliff_texture = (b[6] >> 4) & 0x0F;
}
/* }}} */

/* {{{ w3e_read */
WErr w3e_read(const unsigned char *s, size_t len, W3eGround **out, WWhy *why)
{
    *out = NULL;
    if (len < 4 || memcmp(s, "W3E!", 4))
        return w_fail(why, W_ERR_CORRUPT, "war3map.w3e doesn't start with W3E! (%zu bytes)", len);
    /* version, main tileset, custom flag: 9 bytes */
    if (len < 4 + 9) return w_fail(why, W_ERR_CORRUPT, "war3map.w3e ends inside its head (%zu bytes, the version and main tileset need 13)", len);
    W3eGround *g = calloc(1, sizeof *g);
    if (!g) return w_fail(why, W_ERR_FULL, "no memory for war3map.w3e's ground");
    size_t pos = 4;
    g->version = rd32(s + pos); pos += 4;
    g->tileset_code = (char)s[pos++];
    snprintf(g->tileset, sizeof g->tileset, "unknown_%c", g->tileset_code);
    for (size_t k = 0; k < sizeof TILESETS / sizeof TILESETS[0]; ++k)
        if (TILESETS[k].code == g->tileset_code) snprintf(g->tileset, sizeof g->tileset, "%s", TILESETS[k].name);
    g->custom_tileset = rd32(s + pos) != 0; pos += 4;
    WErr e = ids(s, len, &pos, "ground", &g->nground, &g->ground_ids, why);
    if (!e) e = ids(s, len, &pos, "cliff", &g->ncliff, &g->cliff_ids, why);
    if (e) { w3e_free(g); return e; }
    if (len - pos < 16) {
        w3e_free(g);
        return w_fail(why, W_ERR_CORRUPT, "war3map.w3e ends before its size and corner (at byte %zu of %zu)", pos, len);
    }
    int32_t w = rd32(s + pos), h = rd32(s + pos + 4);
    g->offset_x = rdf(s + pos + 8);
    g->offset_y = rdf(s + pos + 12);
    pos += 16;
    if (w < 1 || h < 1) {
        w3e_free(g);
        return w_fail(why, W_ERR_CORRUPT, "war3map.w3e's ground is %d x %d tile points, not at least 1 x 1", (int)w, (int)h);
    }
    uint64_t need = (uint64_t)w * (uint64_t)h * W3E_POINT_BYTES;
    if (need > len - pos) {
        w3e_free(g);
        return w_fail(why, W_ERR_CORRUPT, "war3map.w3e is %zu bytes; its %d x %d tile points need %llu after its head of %zu",
                      len, (int)w, (int)h, (unsigned long long)need, pos);
    }
    g->width = w;
    g->height = h;
    g->points = malloc((size_t)w * (size_t)h * sizeof *g->points);
    if (!g->points) { w3e_free(g); return w_fail(why, W_ERR_FULL, "no memory for war3map.w3e's %d x %d tile points", (int)w, (int)h); }
    for (size_t k = 0; k < (size_t)w * (size_t)h; ++k, pos += W3E_POINT_BYTES) point(s + pos, &g->points[k]);
    *out = g;
    return W_OK;
}
/* }}} */

/* {{{ w3e_free */
void w3e_free(W3eGround *g)
{
    if (!g) return;
    free(g->ground_ids);
    free(g->cliff_ids);
    free(g->points);
    free(g);
}
/* }}} */

/* {{{ w3e_point, w3e_texture_name, w3e_walkable */
const W3ePoint *w3e_point(const W3eGround *g, int x, int y)
{
    if (x < 0 || x >= g->width || y < 0 || y >= g->height) return NULL;
    return &g->points[(size_t)y * g->width + x];
}

const char *w3e_texture_name(const W3eGround *g, int x, int y)
{
    const W3ePoint *p = w3e_point(g, x, y);
    if (!p || p->ground_texture >= g->nground) return NULL;
    return g->ground_ids + 4 * p->ground_texture;
}

int w3e_walkable(const W3eGround *g, int x, int y)
{
    const W3ePoint *p = w3e_point(g, x, y);
    return p && !p->has_water && !p->boundary && !p->is_boundary;
}
/* }}} */

/* {{{ w3e_ground_z, w3e_water_z, w3e_is_wet */
double w3e_ground_z(const W3ePoint *p) { return (double)p->height + (p->layer_height - 2) * 128; }
double w3e_water_z(const W3ePoint *p) { return (double)p->water_level - W3E_WATER_DROP; }
int w3e_is_wet(const W3ePoint *p) { return p->has_water && w3e_water_z(p) > w3e_ground_z(p); }
/* }}} */

/* {{{ w3e_tile_to_world, w3e_world_to_tile */
void w3e_tile_to_world(const W3eGround *g, int x, int y, double *wx, double *wy)
{
    *wx = x * 128.0 + g->offset_x;
    *wy = y * 128.0 + g->offset_y;
}

void w3e_world_to_tile(const W3eGround *g, double wx, double wy, double *x, double *y)
{
    *x = floor((wx - g->offset_x) / 128);
    *y = floor((wy - g->offset_y) / 128);
}
/* }}} */

/* {{{ w3e_stats */
void w3e_stats(const W3eGround *g, W3eStats *o)
{
    memset(o, 0, sizeof *o);
    o->total = g->width * g->height;
    o->min_height = INFINITY;
    o->max_height = -INFINITY;
    for (int k = 0; k < o->total; ++k) {
        const W3ePoint *p = &g->points[k];
        o->water += p->has_water;
        o->blight += p->is_blight;
        o->ramp += p->is_ramp;
        o->boundary += p->boundary || p->is_boundary;
        if (p->height < o->min_height) o->min_height = p->height;
        if (p->height > o->max_height) o->max_height = p->height;
        ++o->layers[p->layer_height];
    }
}
/* }}} */

/* {{{ w3e_variation */
int w3e_variation(int extended, int v)
{
    if (extended) {
        if (v < 16) return 16 + v;
        return v == 16 ? 15 : 0;
    }
    return v == 0 ? 15 : 0;
}
/* }}} */

/* {{{ static prep_point: one point's heights (the Lua's terrain_arrays: floats) */
static void prep_point(W3ePrep *p, const W3eGround *g, size_t k)
{
    const W3ePoint *tp = &g->points[k];
    p->ground_z[k] = (float)w3e_ground_z(tp);
    p->water_z[k] = tp->has_water ? (float)w3e_water_z(tp) : (float)-1e30;
    p->wet[k] = (uint8_t)w3e_is_wet(tp);
}
/* }}} */

/* {{{ static prep_cell: one cell's kind and layers (ground.lay's skip, then
 * ground.cells); returns 1 when it is textured */
static int prep_cell(W3ePrep *p, const W3eGround *g, int i, int j)
{
    int w = g->width;
    const W3ePoint *c[4] = {                    /* the Lua's order: sw, se, nw, ne */
        &g->points[(size_t)j * w + i], &g->points[(size_t)j * w + i + 1],
        &g->points[(size_t)(j + 1) * w + i], &g->points[(size_t)(j + 1) * w + i + 1],
    };
    size_t cell = (size_t)j * (w - 1) + i;
    uint8_t *o = p->cells + cell * 8;
    memset(o, W3E_NONE, 8);
    /* bare: any corner blighted; or corners on different levels, no ramp */
    int lo = 99, hi = -1, ramp = 0, blight = 0;
    for (int q = 0; q < 4; ++q) {
        blight |= c[q]->is_blight;
        if (c[q]->layer_height < lo) lo = c[q]->layer_height;
        if (c[q]->layer_height > hi) hi = c[q]->layer_height;
        ramp |= c[q]->is_ramp;
    }
    if (blight) { p->cell_kind[cell] = W3E_CELL_BLIGHT; return 0; }
    if (hi > lo && !ramp) { p->cell_kind[cell] = W3E_CELL_CLIFF; return 0; }
    p->cell_kind[cell] = W3E_CELL_TEXTURED;
    int sw = c[0]->ground_texture, se = c[1]->ground_texture, nw = c[2]->ground_texture, ne = c[3]->ground_texture;
    /* the distinct tilesets, lowest first */
    int list[4], n = 0;
    int corner[4] = { sw, se, nw, ne };
    for (int q = 0; q < 4; ++q) {
        int seen = 0;
        for (int k = 0; k < n; ++k) seen |= list[k] == corner[q];
        if (!seen) list[n++] = corner[q];
    }
    for (int a = 1; a < n; ++a)                 /* at most four: a simple insertion sort */
        for (int b = a; b > 0 && list[b - 1] > list[b]; --b) { int t = list[b]; list[b] = list[b - 1]; list[b - 1] = t; }
    int ext = list[0] < p->nextended && p->extended[list[0]];
    o[0] = (uint8_t)list[0];
    o[1] = (uint8_t)w3e_variation(ext, c[0]->texture_details);
    for (int q = 1; q < n; ++q) {
        int t = list[q];
        o[q * 2] = (uint8_t)t;
        o[q * 2 + 1] = (uint8_t)((se == t ? BIT_SE : 0) + (sw == t ? BIT_SW : 0) + (ne == t ? BIT_NE : 0) + (nw == t ? BIT_NW : 0));
    }
    return 1;
}
/* }}} */

/* {{{ w3e_prep */
WErr w3e_prep(const W3eGround *g, const uint8_t *extended, int nextended, W3ePrep **out, WWhy *why)
{
    *out = NULL;
    if (nextended < 0 || (nextended > 0 && !extended))
        return w_fail(why, W_ERR_REFUSED, "w3e_prep: %d extended flags given%s", nextended, extended ? "" : " with no array");
    W3ePrep *p = calloc(1, sizeof *p);
    if (!p) return w_fail(why, W_ERR_FULL, "no memory for the WC3 ground's preparation");
    size_t np = (size_t)g->width * g->height, nc = (size_t)(g->width - 1) * (g->height - 1);
    p->width = g->width;
    p->height = g->height;
    p->nextended = nextended;
    p->extended = malloc(nextended ? (size_t)nextended : 1);
    p->ground_z = malloc(np * sizeof(float));
    p->water_z = malloc(np * sizeof(float));
    p->wet = malloc(np);
    p->cells = malloc(nc ? nc * 8 : 1);
    p->cell_kind = malloc(nc ? nc : 1);
    if (!p->extended || !p->ground_z || !p->water_z || !p->wet || !p->cells || !p->cell_kind) {
        w3e_prep_free(p);
        return w_fail(why, W_ERR_FULL, "no memory to prepare the WC3 ground's %d x %d tile points", g->width, g->height);
    }
    if (nextended) memcpy(p->extended, extended, (size_t)nextended);
    for (size_t k = 0; k < np; ++k) prep_point(p, g, k);
    for (int j = 0; j < g->height - 1; ++j)
        for (int i = 0; i < g->width - 1; ++i) p->used += prep_cell(p, g, i, j);
    *out = p;
    return W_OK;
}
/* }}} */

/* {{{ w3e_prep_area */
WErr w3e_prep_area(W3ePrep *p, const W3eGround *g, int x0, int y0, int x1, int y1, W3eArea *touched, WWhy *why)
{
    if (touched) *touched = (W3eArea){ 0, 0, -1, -1 };
    if (g->width != p->width || g->height != p->height)
        return w_fail(why, W_ERR_REFUSED, "w3e_prep_area: the ground is %d x %d tile points, its preparation %d x %d",
                      g->width, g->height, p->width, p->height);
    int ax0 = x0 < 0 ? 0 : x0, ay0 = y0 < 0 ? 0 : y0;
    int ax1 = x1 >= g->width ? g->width - 1 : x1, ay1 = y1 >= g->height ? g->height - 1 : y1;
    if (ax1 < ax0 || ay1 < ay0)
        return w_fail(why, W_ERR_REFUSED, "w3e_prep_area: tile points %d to %d across, %d to %d up miss the %d x %d grid",
                      x0, x1, y0, y1, g->width, g->height);
    for (int y = ay0; y <= ay1; ++y)
        for (int x = ax0; x <= ax1; ++x) prep_point(p, g, (size_t)y * g->width + x);
    /* the cells with one of those points as a corner */
    int cx0 = ax0 - 1 < 0 ? 0 : ax0 - 1, cy0 = ay0 - 1 < 0 ? 0 : ay0 - 1;
    int cx1 = ax1 > g->width - 2 ? g->width - 2 : ax1, cy1 = ay1 > g->height - 2 ? g->height - 2 : ay1;
    for (int j = cy0; j <= cy1; ++j)
        for (int i = cx0; i <= cx1; ++i) {
            size_t cell = (size_t)j * (g->width - 1) + i;
            p->used -= p->cell_kind[cell] == W3E_CELL_TEXTURED;
            p->used += prep_cell(p, g, i, j);
        }
    if (touched && cx1 >= cx0 && cy1 >= cy0) *touched = (W3eArea){ cx0, cy0, cx1, cy1 };
    return W_OK;
}
/* }}} */

/* {{{ w3e_prep_free */
void w3e_prep_free(W3ePrep *p)
{
    if (!p) return;
    free(p->extended);
    free(p->ground_z);
    free(p->water_z);
    free(p->wet);
    free(p->cells);
    free(p->cell_kind);
    free(p);
}
/* }}} */
