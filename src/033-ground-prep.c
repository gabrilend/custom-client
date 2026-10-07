/*
 * 033-ground-prep.c (issue 173u) - a tile's ground made ready, without the window; see 033-ground-prep.h
 *
 * For a general audience: the arithmetic half of loading a tile's ground
 * (its points, its blend picture, its pictures unpacked), split from the
 * sending half (038-ground.c) so a second thread can do it ahead of her.
 */
#include "033-ground-prep.h"
#include "017-blp.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* {{{ ground_place_init */
int ground_place_init(GroundPlace *p, MpqChain *chain, const char *map_name, MapWorld *world, double ox, double oy, char *why, int why_len)
{
    memset(p, 0, sizeof *p);
    p->chain = chain;
    p->world = world;
    p->ox = ox; p->oy = oy;
    snprintf(p->map_name, sizeof p->map_name, "%s", map_name);
    char path[200];
    snprintf(path, sizeof path, "World\\Maps\\%s\\%s.wdt", map_name, map_name);
    size_t len;
    const char *from;
    unsigned char *wdt = mpq_chain_read(chain, path, &len, &from);
    if (!wdt) { snprintf(why, why_len, "%s is in none of the client's archives", path); return 1; }
    p->big_alpha = adt_wdt_big_alpha(wdt, len, why, why_len);
    free(wdt);
    return p->big_alpha < 0;
}
/* }}} */

/* {{{ ground_prep_pixels */
int ground_prep_pixels(MpqChain *chain, const char *name, GroundPixels *px, char *why, int why_len)
{
    size_t len;
    const char *from;
    unsigned char *s = mpq_chain_read(chain, name, &len, &from);
    if (!s) { snprintf(why, why_len, "the ground picture %s is in none of the client's archives", name); return 1; }
    BlpImage img;
    char inner[300];
    int bad = blp_decode(s, len, &img, inner, sizeof inner);
    free(s);
    if (bad) { snprintf(why, why_len, "the ground picture %s: %s", name, inner); return 1; }
    px->rgba = img.rgba;                                 /* kept: blp_free would only free this */
    px->width = img.width;
    px->height = img.height;
    px->smaller = NULL;
    ground_prep_smaller(px);
    return 0;
}
/* }}} */

/* {{{ ground_prep_smaller
 * Made here, not by the card, because the card can only remake every
 * layer of the shared picture array at once (5 ms a tile, measured
 * 2026-10-07); made here, each new picture's copies cost the main thread
 * only their sending. */
void ground_prep_smaller(GroundPixels *px)
{
    int side = px->width;
    if (side != px->height || side < 2 || (side & (side - 1))) return;   /* not square, or not a power of two */
    size_t total = 0;
    for (int s = side / 2; s >= 1; s /= 2) total += (size_t)s * s * 4;
    px->smaller = malloc(total);
    const unsigned char *from = px->rgba;
    unsigned char *to = px->smaller;
    for (int s = side / 2; s >= 1; s /= 2) {
        int big = s * 2;
        for (int y = 0; y < s; ++y)
            for (int x = 0; x < s; ++x)
                for (int c = 0; c < 4; ++c) {
                    int sum = from[((2 * y) * big + 2 * x) * 4 + c] + from[((2 * y) * big + 2 * x + 1) * 4 + c]
                            + from[((2 * y + 1) * big + 2 * x) * 4 + c] + from[((2 * y + 1) * big + 2 * x + 1) * 4 + c];
                    to[(y * s + x) * 4 + c] = (unsigned char)((sum + 2) / 4);
                }
        from = to;
        to += (size_t)s * s * 4;
    }
}
/* }}} */

/* {{{ static chunk_points: one chunk's 145 points and its triangles,
 * written into a quarter's arrays at point `base` and index `*n`
 * The original's four triangles a cell (each corner pair with the
 * centre), none in a hole: hole bit (row / 2) * 4 + col / 2 hides a 2 x 2
 * block of cells. `bx`, `by` are the chunk's place in the tile's blend
 * picture (0-15 each). */
static void chunk_points(const AdtChunk *c, const GroundPlace *p, GroundQuarter *q, int base, int bx, int by)
{
    const float step = ADT_CHUNK_SIZE / 8;
    for (int i = 0; i < 145; ++i) {
        int   r17 = i % 17, v = base + i;
        float row = (float)(i / 17) + (r17 >= 9 ? 0.5f : 0.0f);      /* centres sit half a cell in */
        float col = r17 >= 9 ? (float)(r17 - 9) + 0.5f : (float)r17;
        float wx = c->x - row * step, wy = c->y - col * step;
        q->vertices[v * 3]     = (float)(wx - p->ox);
        q->vertices[v * 3 + 1] = (float)(wy - p->oy);
        q->vertices[v * 3 + 2] = c->z + c->heights[i];
        /* light: the server's ground's slope a yard either side, so it runs on
         * smoothly over chunk and tile edges */
        float gx, gy;
        float hn = maps_world_height(p->world, wx + 1, wy, &gx, &gy), hs = maps_world_height(p->world, wx - 1, wy, &gx, &gy);
        float hw = maps_world_height(p->world, wx, wy + 1, &gx, &gy), he = maps_world_height(p->world, wx, wy - 1, &gx, &gy);
        /* where the server has no ground a yard away (the map's edge), the
         * point is lit as level ground; elsewhere the slope's normal */
        int   edge = hn < -10000 || hs < -10000 || hw < -10000 || he < -10000;
        float nx = edge ? 0 : -(hn - hs) / 2, ny = edge ? 0 : -(hw - he) / 2;
        float nl = sqrtf(nx * nx + ny * ny + 1);
        q->normals[v * 3] = nx / nl; q->normals[v * 3 + 1] = ny / nl; q->normals[v * 3 + 2] = 1 / nl;
        q->colors[v * 4] = c->tint[i][0]; q->colors[v * 4 + 1] = c->tint[i][1]; q->colors[v * 4 + 2] = c->tint[i][2]; q->colors[v * 4 + 3] = 255;
        q->texcoords[v * 2] = col;          /* 8 repeats across the chunk */
        q->texcoords[v * 2 + 1] = row;
        /* corners land on the chunk's blend square's first and last points' centres */
        q->texcoords2[v * 2]     = ((float)bx * 64 + 0.5f + col / 8.0f * 63.0f) / 1024.0f;
        q->texcoords2[v * 2 + 1] = ((float)by * 64 + 0.5f + row / 8.0f * 63.0f) / 1024.0f;
    }
    for (int r = 0; r < 8; ++r)
        for (int col = 0; col < 8; ++col) {
            if (c->holes & (1 << ((r / 2) * 4 + col / 2))) continue;
            unsigned short a = (unsigned short)(base + adt_corner(r, col)), b = (unsigned short)(base + adt_corner(r, col + 1));
            unsigned short d = (unsigned short)(base + adt_corner(r + 1, col)), e = (unsigned short)(base + adt_corner(r + 1, col + 1));
            unsigned short mid = (unsigned short)(base + adt_centre(r, col));
            unsigned short tri[12] = { a, b, mid, b, e, mid, e, d, mid, d, a, mid };
            memcpy(q->indices + q->nindex, tri, sizeof tri);
            q->nindex += 12;
        }
}
/* }}} */

/* {{{ ground_prep_tile */
int ground_prep_tile(const GroundPlace *p, int tx, int ty, int (*on_card)(const char *name), GroundPrep **out, char *why, int why_len)
{
    *out = NULL;
    if (tx < 0 || tx > 63 || ty < 0 || ty > 63) return 1;
    char path[200];
    snprintf(path, sizeof path, "World\\Maps\\%s\\%s_%d_%d.adt", p->map_name, p->map_name, ty, tx);   /* west-east number first */
    size_t len;
    const char *from;
    unsigned char *s = mpq_chain_read(p->chain, path, &len, &from);
    if (!s) return 1;
    GroundPrep *g = calloc(1, sizeof *g);
    g->tx = tx; g->ty = ty;
    g->adt = malloc(sizeof *g->adt);
    char inner[400];
    if (adt_read(s, len, p->big_alpha, g->adt, inner, sizeof inner)) {
        snprintf(why, why_len, "%s (from %s): %s", path, from, inner);
        free(s); free(g->adt); free(g);
        return -1;
    }
    free(s);
    const AdtTile *adt = g->adt;
    if (adt->ntextures > 256) {
        snprintf(why, why_len, "%s lists %d ground pictures; at most 256 are expected", path, adt->ntextures);
        ground_prep_free(g, 0);
        return -1;
    }
    /* each chunk's four pictures (layers past nlayers repeat layer 0:
     * their strength is 0), checked against the tile's list */
    for (int k = 0; k < 256; ++k) {
        const AdtChunk *c = &adt->chunks[k];
        for (int l = 0; l < 4; ++l) {
            int t = c->tex[l < c->nlayers ? l : 0];
            if (t < 0 || t >= adt->ntextures) {
                snprintf(why, why_len, "%s: chunk %d names picture %d of %d", path, k, t, adt->ntextures);
                ground_prep_free(g, 0);
                return -1;
            }
            g->chunk_tex[k][l] = t;
            g->needed[t] = 1;
        }
    }
    /* the pictures not already on the card */
    for (int t = 0; t < adt->ntextures; ++t) {
        if (!g->needed[t] || (on_card && on_card(adt->textures[t]))) continue;
        if (ground_prep_pixels(p->chain, adt->textures[t], &g->pixels[t], why, why_len)) { ground_prep_free(g, 0); return -1; }
    }
    /* each quarter: 64 chunks of 145 points (9,280, within the 16-bit
     * point numbers), 768 triangle corners a chunk at most */
    g->blend = calloc(1024 * 1024, 4);
    for (int qn = 0; qn < 4; ++qn) {
        GroundQuarter *q = &g->quarter[qn];
        const int npt = 64 * 145;
        q->vertices   = malloc(sizeof(float) * 3 * npt);
        q->normals    = malloc(sizeof(float) * 3 * npt);
        q->texcoords  = malloc(sizeof(float) * 2 * npt);
        q->texcoords2 = malloc(sizeof(float) * 2 * npt);
        q->colors     = malloc(4 * (size_t)npt);
        q->indices    = malloc(sizeof(unsigned short) * 64 * 768);
        float lo[3] = { 1e30f, 1e30f, 1e30f }, hi[3] = { -1e30f, -1e30f, -1e30f };
        for (int qk = 0; qk < 64; ++qk) {
            /* quarter qn holds chunk rows 8 * (qn / 2) .. +7 and columns 8 * (qn % 2) .. +7 */
            int crow = (qn / 2) * 8 + qk / 8, ccol = (qn % 2) * 8 + qk % 8, k = crow * 16 + ccol;
            const AdtChunk *c = &adt->chunks[k];
            int before = q->nindex;
            chunk_points(c, p, q, qk * 145, ccol, crow);
            if (q->nindex > before) q->made = 1;
            for (int i = 0; i < 145; ++i) for (int d = 0; d < 3; ++d) {
                float v = q->vertices[(qk * 145 + i) * 3 + d];
                if (v < lo[d]) lo[d] = v;
                if (v > hi[d]) hi[d] = v;
            }
            for (int i = 0; i < 4096; ++i) {
                size_t o = ((size_t)(crow * 64 + i / 64) * 1024 + (size_t)(ccol * 64 + i % 64)) * 4;
                g->blend[o]     = c->nlayers > 1 ? c->alpha[1][i] : 0;
                g->blend[o + 1] = c->nlayers > 2 ? c->alpha[2][i] : 0;
                g->blend[o + 2] = c->nlayers > 3 ? c->alpha[3][i] : 0;
                g->blend[o + 3] = c->shadow[i] ? 0 : 255;    /* 173h: 255 lit, 0 shadowed */
            }
        }
        for (int d = 0; d < 3; ++d) q->centre[d] = (lo[d] + hi[d]) / 2;
        float dx = hi[0] - q->centre[0], dy = hi[1] - q->centre[1], dz = hi[2] - q->centre[2];
        q->radius = sqrtf(dx * dx + dy * dy + dz * dz);
    }
    *out = g;
    return 0;
}
/* }}} */

/* {{{ ground_prep_free */
void ground_prep_free(GroundPrep *g, int keep_adt)
{
    for (int qn = 0; qn < 4; ++qn) {
        GroundQuarter *q = &g->quarter[qn];
        free(q->vertices); free(q->normals); free(q->texcoords); free(q->texcoords2); free(q->colors); free(q->indices);
    }
    for (int t = 0; t < 256; ++t) { free(g->pixels[t].rgba); free(g->pixels[t].smaller); }
    free(g->blend);
    if (!keep_adt && g->adt) { adt_free(g->adt); free(g->adt); }
    free(g);
}
/* }}} */
