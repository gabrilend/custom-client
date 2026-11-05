/*
 * 033-ground-prep.c (issue 173u) - a tile's ground made ready, without the window; see 033-ground-prep.h
 *
 * For a general audience: the arithmetic half of loading a tile's ground
 * (its points, its blend picture, its pictures unpacked), split from the
 * sending half (038-ground.c) so a second thread can do it ahead of her.
 */
#include "033-ground-prep.h"
#include "013-errors.h"
#include "017-blp.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* {{{ ground_place_init */
WErr ground_place_init(GroundPlace *p, MpqChain *chain, Resolver *assets, const char *map_name, MapWorld *world, double ox, double oy, WWhy *why)
{
    memset(p, 0, sizeof *p);
    p->chain = chain;
    p->assets = assets;
    p->world = world;
    p->ox = ox; p->oy = oy;
    snprintf(p->map_name, sizeof p->map_name, "%s", map_name);
    char path[200];
    snprintf(path, sizeof path, "World\\Maps\\%s\\%s.wdt", map_name, map_name);
    AssetAnswer wdt;
    WErr rc = asset_read(assets, chain, path, ASSET_GROUND, &wdt, why);
    if (rc) return rc == W_ERR_NOT_FOUND ? w_fail(why, rc, "%s is in none of the %s", path, assets ? "places asked (the test map's folder, overlays, the archives)" : "client's archives") : rc;
    p->big_alpha = adt_wdt_big_alpha(wdt.data, wdt.len, why);
    asset_answer_free(&wdt);
    return p->big_alpha < 0 ? why->code : W_OK;
}
/* }}} */

/* {{{ ground_prep_pixels */
WErr ground_prep_pixels(const GroundPlace *p, const char *name, GroundPixels *px, WWhy *why)
{
    AssetAnswer a;
    WWhy inner;
    size_t nl = strlen(name), ml = strlen(WC3_LAYER_MARK);
    if (nl > ml && !strcmp(name + nl - ml, WC3_LAYER_MARK)) {     /* a WC3 tileset's square of subtiles (078) */
        if (!p->wc3) return w_fail(why, W_ERR_NOT_FOUND, "%s is a WC3 layer, and this ground has no WC3 tilesets", name);
        int side;
        if (wc3_ground_layer(p->wc3, name, &px->rgba, &side, &inner)) return w_fail(why, inner.code, "%s", inner.text);
        px->width = px->height = side;
        px->smaller = NULL;
        ground_prep_smaller(px);
        return 0;
    }
    WErr rc = asset_get(p->assets, p->chain, name, ASSET_TEXTURE, &a, &inner);
    if (rc) return w_fail(why, rc, "%s (a ground picture)", inner.text);
    BlpImage img;
    int bad = blp_decode(a.data, a.len, &img, &inner);
    asset_answer_free(&a);
    if (bad) { w_fail(why, inner.code, "the ground picture %s: %s", name, inner.text); return why->code; }
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
WErr ground_prep_tile(const GroundPlace *p, int tx, int ty, int (*on_card)(void *ctx, const char *name), void *ctx, GroundPrep **out, WWhy *why)
{
    *out = NULL;
    if (tx < 0 || tx > 63 || ty < 0 || ty > 63) return w_fail(why, W_ERR_NOT_FOUND, "tile (%d, %d) is off the map's 64 x 64 grid", tx, ty);
    char path[200];
    snprintf(path, sizeof path, "World\\Maps\\%s\\%s_%d_%d.adt", p->map_name, p->map_name, ty, tx);   /* west-east number first */
    AssetAnswer file;
    WErr rc = asset_read(p->assets, p->chain, path, ASSET_GROUND, &file, why);
    if (rc == W_ERR_NOT_FOUND) return w_fail(why, W_ERR_NOT_FOUND, "%s is in none of the %s (open sea: no ground there)", path, p->assets ? "places asked" : "archives");
    if (rc) return rc;
    unsigned char *s = file.data;
    size_t len = file.len;
    const char *from = file.from ? file.from : asset_source_name(file.source);
    GroundPrep *g = calloc(1, sizeof *g);
    g->tx = tx; g->ty = ty;
    g->adt = malloc(sizeof *g->adt);
    WWhy inner;
    if (adt_read(s, len, p->big_alpha, g->adt, &inner)) {
        w_fail(why, inner.code, "%s (from %s): %s", path, from, inner.text);
        free(s); free(g->adt); free(g);
        return why->code;
    }
    free(s);
    const AdtTile *adt = g->adt;
    if (adt->ntextures > 256) {
        w_fail(why, W_ERR_CORRUPT, "%s lists %d ground pictures; at most 256 are expected", path, adt->ntextures);
        ground_prep_free(g, 0);
        return why->code;
    }
    /* each chunk's four pictures (layers past nlayers repeat layer 0:
     * their strength is 0), checked against the tile's list */
    for (int k = 0; k < 256; ++k) {
        const AdtChunk *c = &adt->chunks[k];
        for (int l = 0; l < 4; ++l) {
            int t = c->tex[l < c->nlayers ? l : 0];
            if (t < 0 || t >= adt->ntextures) {
                w_fail(why, W_ERR_CORRUPT, "%s: chunk %d names picture %d of %d", path, k, t, adt->ntextures);
                ground_prep_free(g, 0);
                return why->code;
            }
            g->chunk_tex[k][l] = t;
            g->needed[t] = 1;
        }
    }
    /* a converted map's own WC3 ground: its chunks on the WC3 grid are
     * laid cell by cell with WC3's tilesets instead (078), the pictures
     * needed counted again */
    unsigned char painted[256] = { 0 };
    if (p->wc3) {
        g->wc3_lay = malloc(sizeof *g->wc3_lay * WC3_TILE_CELLS * WC3_TILE_CELLS);
        Wc3Paint laid = { 0 };
        if (wc3_ground_lay_tile(p->wc3, g->adt, g->wc3_lay, g->chunk_tex, painted, &laid, &inner)
            || wc3_ground_place_cliffs(p->wc3, g->adt, NULL, &inner)) {   /* its cliffs, drawn as its doodads (042) */
            w_fail(why, inner.code, "%s: %s", path, inner.text);
            ground_prep_free(g, 0);
            return why->code;
        }
        if (!laid.painted) { free(g->wc3_lay); g->wc3_lay = NULL; }   /* off the WC3 grid: drawn as any tile */
        memset(g->needed, 0, sizeof g->needed);
        for (int k = 0; k < 256; ++k) for (int l = 0; l < 4; ++l) g->needed[g->chunk_tex[k][l]] = 1;
        for (size_t c = 0; g->wc3_lay && c < (size_t)WC3_TILE_CELLS * WC3_TILE_CELLS; ++c)
            for (int q = 0; q < 4 && g->wc3_lay[c].sub[q] != WC3_PIECE_NONE; ++q) g->needed[g->wc3_lay[c].pic[q]] = 1;
    }
    /* the pictures not already on the card */
    for (int t = 0; t < adt->ntextures; ++t) {
        if (!g->needed[t] || (on_card && on_card(ctx, adt->textures[t]))) continue;
        if (ground_prep_pixels(p, adt->textures[t], &g->pixels[t], why)) { ground_prep_free(g, 0); return why->code; }
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
                size_t at = (size_t)(crow * 64 + i / 64) * 1024 + (size_t)(ccol * 64 + i % 64), o = at * 4;
                if (painted[k]) memset(g->blend + o, 0, 3);   /* WC3's cells say what shows (078) */
                else {
                    g->blend[o]     = c->nlayers > 1 ? c->alpha[1][i] : 0;
                    g->blend[o + 1] = c->nlayers > 2 ? c->alpha[2][i] : 0;
                    g->blend[o + 2] = c->nlayers > 3 ? c->alpha[3][i] : 0;
                }
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

/* {{{ ground_prep_cell_texels */
void ground_prep_cell_texels(const GroundPrep *g, const int layer_of[256], unsigned char *out)
{
    for (int r = 0; r < WC3_TILE_CELLS; ++r)
        for (int c = 0; c < WC3_TILE_CELLS; ++c) {
            const Wc3Lay *e = &g->wc3_lay[(size_t)r * WC3_TILE_CELLS + c];
            unsigned char *o = out + ((size_t)r * 2 * WC3_TILE_CELLS + (size_t)c * 2) * 4;
            for (int q = 0; q < 4; ++q) {
                int none = e->sub[q] == WC3_PIECE_NONE;
                o[q] = (unsigned char)(none ? 0 : layer_of[e->pic[q]]);
                o[4 + q] = none ? WC3_PIECE_NONE : e->sub[q];
            }
        }
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
    free(g->wc3_lay);
    if (!keep_adt && g->adt) { adt_free(g->adt); free(g->adt); }
    free(g);
}
/* }}} */
