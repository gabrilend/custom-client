/*
 * 028-collide.c (issue 173q) - the world's solid models; see 028-collide.h
 *
 * For a general audience: a list of triangles and a grid of squares,
 * each square listing the triangles that reach into it. Questions walk
 * the squares a spot or a step touches, and test each triangle once.
 */
#include "028-collide.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* {{{ the store
 * tris: 9 floats a triangle (relative to the origin); stamp: the last
 * question that tested it, so a triangle in several squares is tested
 * once. Squares: a hash of (column, row) into BUCKETS chains. */
#define CELL    4.0f
#define BUCKETS 65536
typedef struct { int cx, cy; int *ids; int n, cap; int next; } Square;
struct CollideWorld {
    double  ox, oy;
    float  *tris;   int ntris, cap;
    int    *tile;                     /* per triangle: the map tile that brought it */
    unsigned *stamp; unsigned asked;
    Square *squares; int nsquares, squares_cap;
    int     head[BUCKETS];            /* first square in each chain, -1 none */
};
static unsigned hash(int cx, int cy) { return ((unsigned)cx * 73856093u ^ (unsigned)cy * 19349663u) % BUCKETS; }
static Square *square(CollideWorld *w, int cx, int cy, int make)
{
    unsigned h = hash(cx, cy);
    for (int i = w->head[h]; i >= 0; i = w->squares[i].next)
        if (w->squares[i].cx == cx && w->squares[i].cy == cy) return &w->squares[i];
    if (!make) return NULL;
    if (w->nsquares == w->squares_cap) {
        w->squares_cap = w->squares_cap ? w->squares_cap * 2 : 4096;
        w->squares = realloc(w->squares, sizeof *w->squares * w->squares_cap);
    }
    Square *s = &w->squares[w->nsquares];
    memset(s, 0, sizeof *s);
    s->cx = cx; s->cy = cy; s->next = w->head[h];
    w->head[h] = w->nsquares++;
    return s;
}
CollideWorld *collide_new(double ox, double oy)
{
    CollideWorld *w = calloc(1, sizeof *w);
    w->ox = ox; w->oy = oy;
    for (int i = 0; i < BUCKETS; ++i) w->head[i] = -1;
    return w;
}
void collide_free(CollideWorld *w)
{
    for (int i = 0; i < w->nsquares; ++i) free(w->squares[i].ids);
    free(w->squares); free(w->tris); free(w->stamp); free(w->tile); free(w);
}
int collide_count(const CollideWorld *w) { return w->ntris; }
/* }}} */

/* {{{ collide_add, collide_drop_tile */
static void file(CollideWorld *w, int id)                /* into every square its outline reaches */
{
    const float *o = w->tris + (size_t)id * 9;
    float lo[2] = { 1e30f, 1e30f }, hi[2] = { -1e30f, -1e30f };
    for (int c = 0; c < 3; ++c)
        for (int k = 0; k < 2; ++k) { lo[k] = fminf(lo[k], o[c * 3 + k]); hi[k] = fmaxf(hi[k], o[c * 3 + k]); }
    for (int cx = (int)floorf(lo[0] / CELL); cx <= (int)floorf(hi[0] / CELL); ++cx)
        for (int cy = (int)floorf(lo[1] / CELL); cy <= (int)floorf(hi[1] / CELL); ++cy) {
            Square *s = square(w, cx, cy, 1);
            if (s->n == s->cap) { s->cap = s->cap ? s->cap * 2 : 8; s->ids = realloc(s->ids, sizeof(int) * s->cap); }
            s->ids[s->n++] = id;
        }
}
void collide_add(CollideWorld *w, const float *tris, int ntri, const float m[16], int tile)
{
    for (int t = 0; t < ntri; ++t) {
        if (w->ntris == w->cap) {
            w->cap = w->cap ? w->cap * 2 : 65536;
            w->tris = realloc(w->tris, sizeof(float) * 9 * w->cap);
            w->stamp = realloc(w->stamp, sizeof(unsigned) * w->cap);
            w->tile = realloc(w->tile, sizeof(int) * w->cap);
        }
        float *o = w->tris + (size_t)w->ntris * 9;
        for (int c = 0; c < 3; ++c) {
            const float *p = tris + (size_t)t * 9 + c * 3;
            for (int k = 0; k < 3; ++k) o[c * 3 + k] = m[k] * p[0] + m[4 + k] * p[1] + m[8 + k] * p[2] + m[12 + k];
        }
        w->stamp[w->ntris] = 0;
        w->tile[w->ntris] = tile;
        file(w, w->ntris);
        ++w->ntris;
    }
}
/* the kept triangles close up, and the grid is filed afresh (a tile drop
 * happens as she crosses a tile's edge: rare, and a few milliseconds) */
void collide_drop_tile(CollideWorld *w, int tile)
{
    int kept = 0;
    for (int i = 0; i < w->ntris; ++i) {
        if (w->tile[i] == tile) continue;
        if (kept != i) { memcpy(w->tris + (size_t)kept * 9, w->tris + (size_t)i * 9, sizeof(float) * 9); w->tile[kept] = w->tile[i]; }
        w->stamp[kept] = 0;
        ++kept;
    }
    if (kept == w->ntris) return;
    w->ntris = kept;
    for (int i = 0; i < w->nsquares; ++i) free(w->squares[i].ids);
    w->nsquares = 0;
    for (int i = 0; i < BUCKETS; ++i) w->head[i] = -1;
    for (int i = 0; i < w->ntris; ++i) file(w, i);
}
/* }}} */

/* {{{ collide_floor
 * Each triangle that isn't standing on edge: is (x, y) inside its
 * outline seen from above, and where is its plane there? */
float collide_floor(CollideWorld *w, double x, double y, double z_top, float *gx, float *gy)
{
    float px = (float)(x - w->ox), py = (float)(y - w->oy), best = -100000.0f;
    Square *s = square(w, (int)floorf(px / CELL), (int)floorf(py / CELL), 0);
    if (!s) return best;
    for (int i = 0; i < s->n; ++i) {
        const float *t = w->tris + (size_t)s->ids[i] * 9;
        float ax = t[3] - t[0], ay = t[4] - t[1], bx = t[6] - t[0], by = t[7] - t[1];
        float d = ax * by - ay * bx;                     /* twice the outline's area, signed */
        if (fabsf(d) < 1e-6f) continue;                  /* on edge from above: a wall, not a floor */
        float qx = px - t[0], qy = py - t[1];
        float u = (qx * by - qy * bx) / d, v = (ax * qy - ay * qx) / d;
        if (u < 0 || v < 0 || u + v > 1) continue;
        float z = t[2] + u * (t[5] - t[2]) + v * (t[8] - t[2]);
        if (z > z_top || z <= best) continue;
        best = z;
        /* the plane's slope: from its normal (n = a x b in 3D) */
        float az = t[5] - t[2], bz = t[8] - t[2];
        float nx = ay * bz - az * by, ny = az * bx - ax * bz, nz = ax * by - ay * bx;
        *gx = -nx / nz; *gy = -ny / nz;
    }
    return best;
}
/* }}} */

/* {{{ collide_ray: every triangle in the squares the line's outline
 * touches, each once (Moller-Trumbore, both faces) */
float collide_ray(CollideWorld *w, double x0, double y0, double z0, double x1, double y1, double z1)
{
    float o[3] = { (float)(x0 - w->ox), (float)(y0 - w->oy), (float)z0 };
    float dir[3] = { (float)(x1 - x0), (float)(y1 - y0), (float)(z1 - z0) };
    float best = 2;
    ++w->asked;
    float lo_x = fminf(o[0], o[0] + dir[0]), hi_x = fmaxf(o[0], o[0] + dir[0]);
    float lo_y = fminf(o[1], o[1] + dir[1]), hi_y = fmaxf(o[1], o[1] + dir[1]);
    for (int cx = (int)floorf(lo_x / CELL); cx <= (int)floorf(hi_x / CELL); ++cx)
        for (int cy = (int)floorf(lo_y / CELL); cy <= (int)floorf(hi_y / CELL); ++cy) {
            Square *s = square(w, cx, cy, 0);
            if (!s) continue;
            for (int i = 0; i < s->n; ++i) {
                int id = s->ids[i];
                if (w->stamp[id] == w->asked) continue;
                w->stamp[id] = w->asked;
                const float *t = w->tris + (size_t)id * 9;
                float e1[3] = { t[3] - t[0], t[4] - t[1], t[5] - t[2] }, e2[3] = { t[6] - t[0], t[7] - t[1], t[8] - t[2] };
                float p[3] = { dir[1] * e2[2] - dir[2] * e2[1], dir[2] * e2[0] - dir[0] * e2[2], dir[0] * e2[1] - dir[1] * e2[0] };
                float det = e1[0] * p[0] + e1[1] * p[1] + e1[2] * p[2];
                if (fabsf(det) < 1e-12f) continue;
                float inv = 1.0f / det, tv[3] = { o[0] - t[0], o[1] - t[1], o[2] - t[2] };
                float u = (tv[0] * p[0] + tv[1] * p[1] + tv[2] * p[2]) * inv;
                if (u < 0 || u > 1) continue;
                float q[3] = { tv[1] * e1[2] - tv[2] * e1[1], tv[2] * e1[0] - tv[0] * e1[2], tv[0] * e1[1] - tv[1] * e1[0] };
                float v = (dir[0] * q[0] + dir[1] * q[1] + dir[2] * q[2]) * inv;
                if (v < 0 || u + v > 1) continue;
                float f = (e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2]) * inv;
                if (f >= 0 && f < best) best = f;
            }
        }
    return best;
}
/* }}} */

/* {{{ collide_wall
 * Rays along the step at three heights (knee, waist, head), each reaching
 * `radius` past the step's end, against every steep triangle near the
 * path (Moller-Trumbore). The nearest hit's facing is the wall's. */
int collide_wall(CollideWorld *w, double x0, double y0, double x1, double y1, double z,
                 float radius, float step, float height, float max_grad, float *nx, float *ny)
{
    float ax = (float)(x0 - w->ox), ay = (float)(y0 - w->oy), bx = (float)(x1 - w->ox), by = (float)(y1 - w->oy);
    float dx = bx - ax, dy = by - ay, len = sqrtf(dx * dx + dy * dy);
    if (len < 1e-6f) return 0;
    float ux = dx / len, uy = dy / len, reach = len + radius;
    float steep_nz2 = 1.0f / (1.0f + max_grad * max_grad);   /* a facing with nz^2 below this is too steep to stand on */
    float heights[3] = { (float)z + step, (float)z + height * 0.5f, (float)z + height - 0.1f };
    ++w->asked;
    float nearest = 1e30f;
    int hit = 0;
    float lo_x = fminf(ax, ax + ux * reach) - radius, hi_x = fmaxf(ax, ax + ux * reach) + radius;
    float lo_y = fminf(ay, ay + uy * reach) - radius, hi_y = fmaxf(ay, ay + uy * reach) + radius;
    for (int cx = (int)floorf(lo_x / CELL); cx <= (int)floorf(hi_x / CELL); ++cx)
        for (int cy = (int)floorf(lo_y / CELL); cy <= (int)floorf(hi_y / CELL); ++cy) {
            Square *s = square(w, cx, cy, 0);
            if (!s) continue;
            for (int i = 0; i < s->n; ++i) {
                int id = s->ids[i];
                if (w->stamp[id] == w->asked) continue;
                w->stamp[id] = w->asked;
                const float *t = w->tris + (size_t)id * 9;
                float e1[3] = { t[3] - t[0], t[4] - t[1], t[5] - t[2] }, e2[3] = { t[6] - t[0], t[7] - t[1], t[8] - t[2] };
                float n[3] = { e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0] };
                float nl2 = n[0] * n[0] + n[1] * n[1] + n[2] * n[2];
                if (nl2 < 1e-12f || n[2] * n[2] >= steep_nz2 * nl2) continue;   /* standable: a floor, not a wall */
                for (int k = 0; k < 3; ++k) {
                    float o[3] = { ax, ay, heights[k] }, dir[3] = { ux, uy, 0 };
                    float p[3] = { dir[1] * e2[2] - dir[2] * e2[1], dir[2] * e2[0] - dir[0] * e2[2], dir[0] * e2[1] - dir[1] * e2[0] };
                    float det = e1[0] * p[0] + e1[1] * p[1] + e1[2] * p[2];
                    if (fabsf(det) < 1e-9f) continue;
                    float inv = 1.0f / det, tv[3] = { o[0] - t[0], o[1] - t[1], o[2] - t[2] };
                    float u = (tv[0] * p[0] + tv[1] * p[1] + tv[2] * p[2]) * inv;
                    if (u < 0 || u > 1) continue;
                    float q[3] = { tv[1] * e1[2] - tv[2] * e1[1], tv[2] * e1[0] - tv[0] * e1[2], tv[0] * e1[1] - tv[1] * e1[0] };
                    float v = (dir[0] * q[0] + dir[1] * q[1] + dir[2] * q[2]) * inv;
                    if (v < 0 || u + v > 1) continue;
                    float dist = (e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2]) * inv;
                    if (dist < 0 || dist > reach || dist >= nearest) continue;
                    nearest = dist;
                    hit = 1;
                    float hl = sqrtf(n[0] * n[0] + n[1] * n[1]);
                    float fx = n[0] / hl, fy = n[1] / hl;
                    if (fx * ux + fy * uy > 0) { fx = -fx; fy = -fy; }   /* facing her, whichever way it was wound */
                    *nx = fx; *ny = fy;
                }
            }
        }
    return hit;
}
/* }}} */
