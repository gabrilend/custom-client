/*
 * 027-liquid.c (issue 173r) - where the water is; see 027-liquid.h
 *
 * For a general audience: one list of water layers per chunk, found by
 * the chunk a spot lies in; within a layer's wet cell, the surface is
 * blended between the cell's corners.
 */
#include "027-liquid.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* {{{ the store: per chunk of the world (64 tiles x 16 chunks a side),
 * the layers on it, in a hash of chains */
#define BUCKETS 16384
typedef struct { int gx, gy; AdtLiquid q; float x0, y0; int next; int tile; } Layer;   /* x0, y0: the chunk's north-west corner */
/* a building's pool: its grid (copied), its placement in the world's own
 * coordinates (at: Raylib's layout; inv: the inverse of its turn and
 * size), its world box, kind and tile */
typedef struct { WmoPool p; float at[16], inv[9]; double lo[2], hi[2]; float mid_z; int kind, tile; } Pool;
struct LiquidWorld { Layer *layers; int n, cap; int head[BUCKETS]; Pool *pools; int npools, pools_cap; };
static unsigned hash(int gx, int gy) { return ((unsigned)gx * 2654435761u ^ (unsigned)gy * 40503u) % BUCKETS; }
LiquidWorld *liquid_new(void)
{
    LiquidWorld *w = calloc(1, sizeof *w);
    for (int i = 0; i < BUCKETS; ++i) w->head[i] = -1;
    return w;
}
void liquid_free(LiquidWorld *w)
{
    for (int i = 0; i < w->npools; ++i) { free(w->pools[i].p.height); free(w->pools[i].p.cell); }
    free(w->pools); free(w->layers); free(w);
}
/* }}} */

/* {{{ liquid_add_tile */
void liquid_add_tile(LiquidWorld *w, int tx, int ty, const AdtTile *t)
{
    for (int i = 0; i < t->nliquids; ++i) {
        const AdtLiquid *q = &t->liquids[i];
        const AdtChunk *c = &t->chunks[q->chunk];
        if (w->n == w->cap) { w->cap = w->cap ? w->cap * 2 : 256; w->layers = realloc(w->layers, sizeof *w->layers * w->cap); }
        Layer *l = &w->layers[w->n];
        /* the world's chunk grid: rows run south with world x, columns
         * east with world y, as tiles do (018-maps.h) */
        l->gx = tx * 16 + c->iy;
        l->gy = ty * 16 + c->ix;
        l->q = *q;
        l->tile = tx * 64 + ty;
        l->x0 = c->x;
        l->y0 = c->y;
        unsigned h = hash(l->gx, l->gy);
        l->next = w->head[h];
        w->head[h] = w->n++;
    }
}
/* }}} */

/* {{{ liquid_add_pool
 * The world's coordinates are the placement's plus the drawing origin.
 * The inverse of the turn-and-size part is kept for turning world spots
 * back into the building's space; a placement that can't be inverted (no
 * size) is said and left out. */
static void place(const float m[16], float x, float y, float z, double out[3])
{
    for (int r = 0; r < 3; ++r) out[r] = m[r] * x + m[4 + r] * y + m[8 + r] * z + m[12 + r];
}
void liquid_add_pool(LiquidWorld *w, int tile, const float at[16], double ox, double oy, const WmoPool *p, int kind)
{
    float a = at[0], b = at[4], c = at[8], d = at[1], e = at[5], f = at[9], g = at[2], h = at[6], k = at[10];
    float det = a * (e * k - f * h) - b * (d * k - f * g) + c * (d * h - e * g);
    if (fabsf(det) < 1e-9f) { fprintf(stderr, "liquid: a building's pool has a placement with no size; left out\n"); return; }
    if (w->npools == w->pools_cap) { w->pools_cap = w->pools_cap ? w->pools_cap * 2 : 16; w->pools = realloc(w->pools, sizeof *w->pools * w->pools_cap); }
    Pool *o = &w->pools[w->npools++];
    o->p = *p;
    size_t np = (size_t)p->nx * p->ny, nc = (size_t)(p->nx - 1) * (p->ny - 1);
    o->p.height = malloc(sizeof(float) * np); memcpy(o->p.height, p->height, sizeof(float) * np);
    o->p.cell = malloc(nc); memcpy(o->p.cell, p->cell, nc);
    memcpy(o->at, at, sizeof o->at);
    o->at[12] += (float)ox; o->at[13] += (float)oy;   /* (float: within a yard's thousandth at the map's 17,000-yard edge) */
    /* the inverse, rows of (a b c / d e f / g h k) as the turn's rows */
    float inv[9] = { e * k - f * h, c * h - b * k, b * f - c * e,
                     f * g - d * k, a * k - c * g, c * d - a * f,
                     d * h - e * g, b * g - a * h, a * e - b * d };
    for (int i = 0; i < 9; ++i) o->inv[i] = inv[i] / det;
    o->lo[0] = o->lo[1] = 1e30; o->hi[0] = o->hi[1] = -1e30;
    double zsum = 0;
    for (int yy = 0; yy < p->ny; yy += p->ny - 1)
        for (int xx = 0; xx < p->nx; xx += p->nx - 1) {
            double q[3];
            place(o->at, p->corner[0] + xx * WMO_POOL_CELL, p->corner[1] + yy * WMO_POOL_CELL, p->height[yy * p->nx + xx], q);
            for (int j = 0; j < 2; ++j) { if (q[j] < o->lo[j]) o->lo[j] = q[j]; if (q[j] > o->hi[j]) o->hi[j] = q[j]; }
            zsum += q[2];
        }
    o->mid_z = (float)(zsum / 4);
    o->kind = kind;
    o->tile = tile;
}
/* }}} */

/* {{{ liquid_drop_tile: the kept layers close up and are chained afresh;
 * the tile's pools go */
void liquid_drop_tile(LiquidWorld *w, int tx, int ty)
{
    int tile = tx * 64 + ty, kept = 0;
    for (int i = 0; i < w->npools; ++i) {
        if (w->pools[i].tile == tile) { free(w->pools[i].p.height); free(w->pools[i].p.cell); continue; }
        w->pools[kept++] = w->pools[i];
    }
    w->npools = kept;
    kept = 0;
    for (int i = 0; i < w->n; ++i) if (w->layers[i].tile != tile) w->layers[kept++] = w->layers[i];
    if (kept == w->n) return;
    w->n = kept;
    for (int i = 0; i < BUCKETS; ++i) w->head[i] = -1;
    for (int i = 0; i < w->n; ++i) {
        unsigned h = hash(w->layers[i].gx, w->layers[i].gy);
        w->layers[i].next = w->head[h];
        w->head[h] = i;
    }
}
/* }}} */

/* {{{ liquid_surface */
float liquid_surface(const LiquidWorld *w, double x, double y, int *kind)
{
    const double grid = 533.33333 / 16, step = grid / 8;
    int gx = (int)floor((32.0 * 533.33333 - x) / grid), gy = (int)floor((32.0 * 533.33333 - y) / grid);
    float best = -100000.0f;
    for (int i = w->head[hash(gx, gy)]; i >= 0; i = w->layers[i].next) {
        const Layer *l = &w->layers[i];
        if (l->gx != gx || l->gy != gy) continue;
        double fr = (l->x0 - x) / step, fc = (l->y0 - y) / step;   /* cell row and column, with fractions */
        int r = (int)fr, c = (int)fc;
        if (r < 0 || r > 7 || c < 0 || c > 7 || !l->q.wet[r][c]) continue;
        float u = (float)(fc - c), v = (float)(fr - r);
        float h = (l->q.height[r][c] * (1 - u) + l->q.height[r][c + 1] * u) * (1 - v)
                + (l->q.height[r + 1][c] * (1 - u) + l->q.height[r + 1][c + 1] * u) * v;
        if (h > best) { best = h; *kind = l->q.kind; }
    }
    /* the pools: the spot (at the pool's middle height: buildings stand
     * upright, so the height doesn't move it across the grid) turned into
     * the building's space, its cell, the surface blended over the cell's
     * corners, and that point turned back to find its world height */
    for (int i = 0; i < w->npools; ++i) {
        const Pool *o = &w->pools[i];
        if (x < o->lo[0] || x > o->hi[0] || y < o->lo[1] || y > o->hi[1]) continue;
        double d[3] = { x - o->at[12], y - o->at[13], o->mid_z - o->at[14] };
        double bx = o->inv[0] * d[0] + o->inv[1] * d[1] + o->inv[2] * d[2];
        double by = o->inv[3] * d[0] + o->inv[4] * d[1] + o->inv[5] * d[2];
        double fx = (bx - o->p.corner[0]) / WMO_POOL_CELL, fy = (by - o->p.corner[1]) / WMO_POOL_CELL;
        int cx = (int)floor(fx), cy = (int)floor(fy), nx = o->p.nx;
        if (cx < 0 || cy < 0 || cx >= nx - 1 || cy >= o->p.ny - 1 || wmo_pool_dry(o->p.cell[cy * (nx - 1) + cx])) continue;
        float u = (float)(fx - cx), v = (float)(fy - cy);
        const float *hg = o->p.height;
        float hb = (hg[cy * nx + cx] * (1 - u) + hg[cy * nx + cx + 1] * u) * (1 - v)
                 + (hg[(cy + 1) * nx + cx] * (1 - u) + hg[(cy + 1) * nx + cx + 1] * u) * v;
        double q[3];
        place(o->at, (float)bx, (float)by, hb, q);
        if (q[2] > best) { best = (float)q[2]; *kind = o->kind; }
    }
    return best;
}
/* }}} */
