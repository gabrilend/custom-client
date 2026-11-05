/*
 * 078-wc3-ground.c (issues 302, 1109) - a converted map's ground painted
 * with Warcraft III's own tilesets; see 078-wc3-ground.h
 *
 * For a general audience: at the start the map's WC3 ground, the
 * tileset sheet and every tileset picture are read once; painting a tile
 * then only looks things up, cell by cell.
 */
#include "078-wc3-ground.h"
#include "017-blp.h"
#include "077-slk.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TILESETS_MAX 16                  /* a tile point names 0 to 15 (063: four bits) */
#define W3E_NAME "war3map.w3e"           /* at the build folder's root, as the converter would copy it */
#define SHEET    "TerrainArt\\Terrain.slk"

/* {{{ the handle */
typedef struct { Wc3Tileset t; unsigned char *rgba; } Tileset;
struct Wc3Ground {
    W3eGround *g;
    W3ePrep   *prep;
    Tileset    sets[TILESETS_MAX];
    int        nsets;
    double     scale, x0, y0;
};
/* }}} */

/* {{{ wc3_ground_open, wc3_ground_free */
void wc3_ground_free(Wc3Ground *w)
{
    if (!w) return;
    for (int k = 0; k < w->nsets; ++k) free(w->sets[k].rgba);
    w3e_prep_free(w->prep);
    w3e_free(w->g);
    free(w);
}

WErr wc3_ground_open(Resolver *r, MpqChain *chain, double scale, double x0, double y0, Wc3Ground **out, WWhy *why)
{
    *out = NULL;
    if (!r) return w_fail(why, W_ERR_NOT_FOUND, "no resolver, so no converted map's WC3 ground (a test-map run's)");
    if (!(scale > 0)) return w_fail(why, W_ERR_REFUSED, "a WC3 ground with %g yards a WC3 unit", scale);
    WWhy inner;
    AssetAnswer a;
    WErr rc = asset_read(r, chain, W3E_NAME, ASSET_GROUND, &a, &inner);
    if (rc == W_ERR_NOT_FOUND) return w_fail(why, rc, "no %s in the build folder or any other place (world-edit-to-execute's converter doesn't copy the map's own ground into its build yet)", W3E_NAME);
    if (rc) return w_fail(why, rc, "%s: %s", W3E_NAME, inner.text);
    Wc3Ground *w = calloc(1, sizeof *w);
    w->scale = scale; w->x0 = x0; w->y0 = y0;
    rc = w3e_read(a.data, a.len, &w->g, &inner);
    asset_answer_free(&a);
    if (rc) { wc3_ground_free(w); return w_fail(why, rc, "%s: %s", W3E_NAME, inner.text); }

    rc = asset_read(r, chain, SHEET, ASSET_TABLE, &a, &inner);
    if (rc == W_ERR_NOT_FOUND) { wc3_ground_free(w); return w_fail(why, rc, "no %s in any place (no Warcraft III install: the assetWc3 setting), so its tilesets' pictures aren't known", SHEET); }
    if (rc) { wc3_ground_free(w); return w_fail(why, rc, "%s: %s", SHEET, inner.text); }
    Slk *sheet;
    rc = slk_read(a.data, a.len, &sheet, &inner);
    asset_answer_free(&a);
    if (rc) { wc3_ground_free(w); return w_fail(why, rc, "%s: %s", SHEET, inner.text); }

    /* each tileset a point can name: its picture, the checker when no place has it */
    w->nsets = w->g->nground < TILESETS_MAX ? w->g->nground : TILESETS_MAX;
    uint8_t extended[TILESETS_MAX] = { 0 };
    for (int k = 0; k < w->nsets && !rc; ++k) {
        Tileset *ts = &w->sets[k];
        memcpy(ts->t.id, w->g->ground_ids + 4 * k, 4);
        ts->t.id[4] = '\0';
        const char *dir = slk_value(sheet, ts->t.id, "dir"), *file = slk_value(sheet, ts->t.id, "file");
        if (dir && file) snprintf(ts->t.path, sizeof ts->t.path, "%s\\%s.blp", dir, file);
        char named[128];
        snprintf(named, sizeof named, "%s", ts->t.path[0] ? ts->t.path : "");
        if (!named[0]) snprintf(named, sizeof named, "TerrainArt\\%s.blp", ts->t.id);
        snprintf(ts->t.layer, sizeof ts->t.layer, "%s%s", named, WC3_LAYER_MARK);
        rc = ts->t.path[0] ? asset_get(r, chain, ts->t.path, ASSET_TEXTURE, &a, &inner)
                           : asset_stand_in(r, named, ASSET_TEXTURE, &a, &inner);   /* the sheet has no row for it */
        if (rc) { rc = w_fail(why, rc, "the tileset %s: %s", ts->t.id, inner.text); break; }
        BlpImage img;
        ts->t.source = a.source;
        rc = blp_decode(a.data, a.len, &img, &inner);
        asset_answer_free(&a);
        if (rc) { rc = w_fail(why, rc, "the tileset %s's picture %s: %s", ts->t.id, named, inner.text); break; }
        if (img.width < 4 || img.height < 4) { blp_free(&img); rc = w_fail(why, W_ERR_CORRUPT, "the tileset %s's picture %s is %d x %d, too small for 4 x 4 subtiles", ts->t.id, named, img.width, img.height); break; }
        ts->rgba = img.rgba;                             /* kept: blp_free would only free this */
        ts->t.width = img.width; ts->t.height = img.height;
        ts->t.extended = extended[k] = img.width > img.height;
    }
    slk_free(sheet);
    if (!rc) {
        rc = w3e_prep(w->g, extended, w->nsets, &w->prep, &inner);
        if (rc) rc = w_fail(why, rc, "%s: %s", W3E_NAME, inner.text);
    }
    if (rc) { wc3_ground_free(w); return rc; }
    *out = w;
    return W_OK;
}
/* }}} */

/* {{{ wc3_ground_tilesets, wc3_ground_tileset, wc3_ground_w3e */
int wc3_ground_tilesets(const Wc3Ground *w) { return w->nsets; }
const Wc3Tileset *wc3_ground_tileset(const Wc3Ground *w, int k) { return k >= 0 && k < w->nsets ? &w->sets[k].t : NULL; }
const W3eGround *wc3_ground_w3e(const Wc3Ground *w) { return w->g; }
/* }}} */

/* {{{ wc3_ground_cell_at */
int wc3_ground_cell_at(const Wc3Ground *w, double x, double y, int *i, int *j, double *fx, double *fy)
{
    double wc3_y = (x - w->x0) / w->scale, wc3_x = (w->y0 - y) / w->scale;   /* map.lua's rule, turned round */
    double tx = (wc3_x - w->g->offset_x) / 128.0, ty = (wc3_y - w->g->offset_y) / 128.0;
    double ci = floor(tx), cj = floor(ty);
    if (!(ci >= 0 && ci < w->g->width - 1 && cj >= 0 && cj < w->g->height - 1)) return 0;
    *i = (int)ci; *j = (int)cj;
    if (fx) *fx = tx - ci;
    if (fy) *fy = ty - cj;
    return 1;
}
/* }}} */

/* {{{ wc3_ground_layer */
WErr wc3_ground_layer(const Wc3Ground *w, const char *name, unsigned char **rgba, int *side, WWhy *why)
{
    *rgba = NULL; *side = 0;
    for (int k = 0; k < w->nsets; ++k) {
        const Tileset *ts = &w->sets[k];
        if (strcmp(ts->t.layer, name)) continue;
        int sub = ts->t.height / 4;                      /* subtile 15: column 3, row 3 of the left half */
        unsigned char *o = malloc((size_t)sub * sub * 4);
        for (int y = 0; y < sub; ++y) {
            memcpy(o + (size_t)y * sub * 4, ts->rgba + ((size_t)(3 * sub + y) * ts->t.width + 3 * sub) * 4, (size_t)sub * 4);
            for (int x = 0; x < sub; ++x) o[((size_t)y * sub + x) * 4 + 3] = 255;
        }
        *rgba = o; *side = sub;
        return W_OK;
    }
    return w_fail(why, W_ERR_NOT_FOUND, "no WC3 tileset's layer is named %s", name);
}
/* }}} */

/* {{{ wc3_ground_paint_tile */
/* a cell's (tileset, subtile) pairs, lowest first: 063's, or for a bare
 * (cliff or blighted) cell its south-west point's tileset whole */
typedef struct { int n; uint8_t t[4], s[4]; int inside, bare; } Cell;
static Cell cell_pairs(const Wc3Ground *w, int i, int j)
{
    Cell c = { 0 };
    c.inside = 1;
    int cw = w->g->width - 1;
    const uint8_t *e = w->prep->cells + ((size_t)j * cw + i) * 8;
    if (w->prep->cell_kind[(size_t)j * cw + i] != W3E_CELL_TEXTURED) {
        c.bare = 1;
        int t = w3e_point(w->g, i, j)->ground_texture;
        if (t < w->nsets) { c.t[0] = (uint8_t)t; c.s[0] = 15; c.n = 1; }
        return c;
    }
    for (int q = 0; q < 4 && e[q * 2] != W3E_NONE; ++q)
        if (e[q * 2] < w->nsets) { c.t[c.n] = e[q * 2]; c.s[c.n] = e[q * 2 + 1]; ++c.n; }
    return c;
}

/* a subtile's alpha at (fx east, fy south from its north-west corner, 0 to 1) */
static int subtile_alpha(const Tileset *ts, int s, double fx, double fy)
{
    int sub = ts->t.height / 4;
    int col = s % 4, row = s / 4;
    int px = (int)(fx * sub), py = (int)(fy * sub);
    if (px > sub - 1) px = sub - 1;
    if (py > sub - 1) py = sub - 1;
    return ts->rgba[((size_t)(row * sub + py) * ts->t.width + col * sub + px) * 4 + 3];
}

/* the tile's picture number for a name, added when new */
static int picture_of(AdtTile *adt, const char *name)
{
    for (int t = 0; t < adt->ntextures; ++t) if (!strcmp(adt->textures[t], name)) return t;
    if (adt->ntextures >= 256) return -1;
    char (*grown)[128] = realloc(adt->textures, sizeof *adt->textures * (size_t)(adt->ntextures + 1));
    if (!grown) return -1;
    adt->textures = grown;
    snprintf(adt->textures[adt->ntextures], sizeof adt->textures[adt->ntextures], "%s", name);
    return adt->ntextures++;
}

WErr wc3_ground_paint_tile(const Wc3Ground *w, AdtTile *adt, int chunk_tex[256][4], unsigned char *rgb, unsigned char painted[256], Wc3Paint *counts, WWhy *why)
{
    Wc3Paint none = { 0 };
    if (!counts) counts = &none;
    const float step = ADT_CHUNK_SIZE / 8;
    for (int k = 0; k < 256; ++k) {
        painted[k] = 0;
        const AdtChunk *c = &adt->chunks[k];
        int crow = k / 16, ccol = k % 16;
        /* the chunk's 64 cells (r south, cc east), each by its centre */
        Cell cells[8][8];
        int use[TILESETS_MAX] = { 0 }, inside = 0;
        for (int r = 0; r < 8; ++r)
            for (int cc = 0; cc < 8; ++cc) {
                int i, j;
                double x = c->x - (r + 0.5) * step, y = c->y - (cc + 0.5) * step;
                if (!wc3_ground_cell_at(w, x, y, &i, &j, NULL, NULL)) { memset(&cells[r][cc], 0, sizeof cells[r][cc]); continue; }
                cells[r][cc] = cell_pairs(w, i, j);
                ++inside;
                for (int q = 0; q < cells[r][cc].n; ++q) ++use[cells[r][cc].t[q]];
            }
        if (!inside) continue;                            /* off the WC3 grid: the converter's own pictures */
        /* the four tilesets used most, then drawn lowest number first */
        int keep[4], nkeep = 0;
        int taken[TILESETS_MAX] = { 0 };
        for (; nkeep < 4; ++nkeep) {
            int best = -1;
            for (int t = 0; t < w->nsets; ++t) if (use[t] && !taken[t] && (best < 0 || use[t] > use[best])) best = t;
            if (best < 0) break;
            taken[best] = 1;
            keep[nkeep] = best;
        }
        if (!nkeep) continue;                             /* every cell inside bare with no tileset named: left as it was */
        for (int a = 1; a < nkeep; ++a)
            for (int b = a; b > 0 && keep[b - 1] > keep[b]; --b) { int s = keep[b]; keep[b] = keep[b - 1]; keep[b - 1] = s; }
        int layer_of[TILESETS_MAX];
        for (int t = 0; t < TILESETS_MAX; ++t) layer_of[t] = -1;
        int pics[4];
        for (int l = 0; l < nkeep; ++l) {
            layer_of[keep[l]] = l;
            pics[l] = picture_of(adt, w->sets[keep[l]].t.layer);
            if (pics[l] < 0) return w_fail(why, W_ERR_FULL, "the tile's %d pictures and its WC3 layers come to more than 256", adt->ntextures);
        }
        for (int l = 0; l < 4; ++l) chunk_tex[k][l] = pics[l < nkeep ? l : 0];
        /* each cell's kept pairs: the first whole, the others by their subtile's alpha */
        for (int r = 0; r < 8; ++r)
            for (int cc = 0; cc < 8; ++cc) {
                Cell *e = &cells[r][cc];
                if (!e->inside) { ++counts->outside; continue; }
                ++counts->cells;
                counts->bare += e->bare;
                int n = 0, dropped = 0;
                for (int q = 0; q < e->n; ++q) {
                    if (layer_of[e->t[q]] < 0) { dropped = 1; continue; }
                    e->t[n] = e->t[q]; e->s[n] = (uint8_t)(n ? e->s[q] : 15); ++n;
                }
                e->n = n;
                counts->folded += dropped;
            }
        /* the blend picture's 64 x 64 points: point p lies at 8 p / 63 cells in */
        for (int py = 0; py < 64; ++py)
            for (int px = 0; px < 64; ++px) {
                double rf = py * 8.0 / 63.0, cf = px * 8.0 / 63.0;
                int r = rf >= 8 ? 7 : (int)rf, cc = cf >= 8 ? 7 : (int)cf;
                const Cell *e = &cells[r][cc];
                unsigned char *o = rgb + ((size_t)(crow * 64 + py) * 1024 + (size_t)(ccol * 64 + px)) * 3;
                o[0] = o[1] = o[2] = 0;
                for (int q = 0; q < e->n; ++q) {
                    int l = layer_of[e->t[q]];
                    if (l == 0) continue;                 /* the bottom layer has no strength of its own */
                    o[l - 1] = (unsigned char)(q == 0 ? 255 : subtile_alpha(&w->sets[e->t[q]], e->s[q], cf - cc, rf - r));
                }
            }
        painted[k] = 1;
        ++counts->painted;
    }
    return W_OK;
}
/* }}} */
