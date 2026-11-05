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
#include "079-wc3-cliffs.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define POINT_SETS   16                  /* a tile point names 0 to 15 (063: four bits) */
#define TILESETS_MAX 32                  /* those, the ground under each cliff tileset when the map lists it not, and the blight */
#define W3E_NAME "war3map.w3e"           /* at the build folder's root, where the converter copies it */
#define SHEET    "TerrainArt\\Terrain.slk"
#define EDIT_DATA "UI\\WorldEditData.txt"      /* the editor's data: [TileSets] names each main tileset's blight */

/* {{{ the handle */
typedef struct { Wc3Tileset t; unsigned char *rgba; } Tileset;
struct Wc3Ground {
    W3eGround *g;
    W3ePrep   *prep;
    Tileset    sets[TILESETS_MAX];
    int        nsets, npoint;            /* all, and those the points name (the map's ground list) */
    Wc3Cliffs *cliffs;                   /* NULL: no cliff sheet in any place */
    int        cliff_ground[16];         /* per cliff tileset: the tileset laid under it, or -1 */
    int        blight;                   /* the blight's tileset, or -1: none named */
    double     scale, x0, y0;
};
/* }}} */

/* {{{ wc3_ground_open, wc3_ground_free */
void wc3_ground_free(Wc3Ground *w)
{
    if (!w) return;
    for (int k = 0; k < w->nsets; ++k) free(w->sets[k].rgba);
    wc3_cliffs_free(w->cliffs);
    w3e_prep_free(w->prep);
    w3e_free(w->g);
    free(w);
}

/* a tileset's picture by its id, through the sheet's row (the checker
 * when it has none, or no place has the picture) */
static WErr load_tileset(Resolver *r, MpqChain *chain, const Slk *sheet, const char *id, const char *path, Tileset *ts, WWhy *why)
{
    WWhy inner;
    AssetAnswer a;
    memcpy(ts->t.id, id, 4);
    ts->t.id[4] = '\0';
    const char *dir = sheet ? slk_value(sheet, ts->t.id, "dir") : NULL, *file = sheet ? slk_value(sheet, ts->t.id, "file") : NULL;
    if (path) snprintf(ts->t.path, sizeof ts->t.path, "%s", path);   /* named by the caller (the blight's) */
    else if (dir && file) snprintf(ts->t.path, sizeof ts->t.path, "%s\\%s.blp", dir, file);
    char named[128];
    snprintf(named, sizeof named, "%s", ts->t.path[0] ? ts->t.path : "");
    if (!named[0]) snprintf(named, sizeof named, "TerrainArt\\%s.blp", ts->t.id);
    snprintf(ts->t.layer, sizeof ts->t.layer, "%s%s", named, WC3_LAYER_MARK);
    WErr rc = ts->t.path[0] ? asset_get(r, chain, ts->t.path, ASSET_TEXTURE, &a, &inner)
                            : asset_stand_in(r, named, ASSET_TEXTURE, &a, &inner);   /* the sheet has no row for it */
    if (rc) return w_fail(why, rc, "the tileset %s: %s", ts->t.id, inner.text);
    BlpImage img;
    ts->t.source = a.source;
    rc = blp_decode(a.data, a.len, &img, &inner);
    asset_answer_free(&a);
    if (rc) return w_fail(why, rc, "the tileset %s's picture %s: %s", ts->t.id, named, inner.text);
    if (img.width < 4 || img.height < 4) { blp_free(&img); return w_fail(why, W_ERR_CORRUPT, "the tileset %s's picture %s is %d x %d, too small for 4 x 4 subtiles", ts->t.id, named, img.width, img.height); }
    ts->rgba = img.rgba;                                 /* kept: blp_free would only free this */
    ts->t.width = img.width; ts->t.height = img.height;
    ts->t.extended = img.width > img.height;
    if (img.width >= 2 * img.height) snprintf(ts->t.layer_right, sizeof ts->t.layer_right, "%s%s%s", named, WC3_RIGHT_MARK, WC3_LAYER_MARK);
    return W_OK;
}

/* the blight picture of main tileset `code`: UI\WorldEditData.txt's
 * [TileSets] row for it, its second field (".blp" added when it has no
 * ending); W_ERR_NOT_FOUND when no place has the file or it names none */
static WErr blight_path(Resolver *r, MpqChain *chain, char code, char *out, size_t n, WWhy *why)
{
    AssetAnswer a;
    WWhy inner;
    WErr rc = asset_read(r, chain, EDIT_DATA, ASSET_TABLE, &a, &inner);
    if (rc == W_ERR_NOT_FOUND) return w_fail(why, rc, "no %s in any place", EDIT_DATA);
    if (rc) return w_fail(why, rc, "%s: %s", EDIT_DATA, inner.text);
    const char *p = (const char *)a.data, *end = p + a.len;
    int in = 0, got = 0;
    while (p < end && !got) {
        const char *eol = memchr(p, '\n', (size_t)(end - p));
        const char *stop = eol ? eol : end, *next = eol ? eol + 1 : end;
        while (stop > p && (stop[-1] == '\r' || stop[-1] == ' ' || stop[-1] == '\t')) --stop;
        while (p < stop && (*p == ' ' || *p == '\t')) ++p;
        if (p < stop && *p == '[') in = (size_t)(stop - p) == 10 && !strncasecmp(p, "[TileSets]", 10);
        else if (in && stop - p >= 2 && p[0] == code && memchr(p + 1, '=', (size_t)(stop - p - 1))
                 && strspn(p + 1, " \t") == (size_t)((const char *)memchr(p + 1, '=', (size_t)(stop - p - 1)) - p - 1)) {   /* "A=" or "A = " */
            const char *f = memchr(p, ',', (size_t)(stop - p));     /* the second field */
            if (f) {
                const char *fe = memchr(f + 1, ',', (size_t)(stop - f - 1));
                if (!fe) fe = stop;
                const char *fs = f + 1;
                while (fs < fe && (*fs == ' ' || *fs == '"')) ++fs;
                while (fe > fs && (fe[-1] == ' ' || fe[-1] == '"')) --fe;
                size_t len = (size_t)(fe - fs);
                int ends = len > 4 && !strncasecmp(fe - 4, ".blp", 4);
                if (len && len + 5 < n) { snprintf(out, n, "%.*s%s", (int)len, fs, ends ? "" : ".blp"); got = 1; }
            }
            if (!got) break;
        }
        p = next;
    }
    asset_answer_free(&a);
    return got ? W_OK : w_fail(why, W_ERR_NOT_FOUND, "%s names no blight for main tileset %c", EDIT_DATA, code);
}

WErr wc3_ground_open(Resolver *r, MpqChain *chain, double scale, double x0, double y0, Wc3Ground **out, WWhy *why)
{
    *out = NULL;
    if (!r) return w_fail(why, W_ERR_NOT_FOUND, "no resolver, so no converted map's WC3 ground (a test-map run's)");
    if (!(scale > 0)) return w_fail(why, W_ERR_REFUSED, "a WC3 ground with %g yards a WC3 unit", scale);
    WWhy inner;
    AssetAnswer a;
    WErr rc = asset_read(r, chain, W3E_NAME, ASSET_GROUND, &a, &inner);
    if (rc == W_ERR_NOT_FOUND) return w_fail(why, rc, "no %s in the build folder or any other place (world-edit-to-execute's converter copies the map's own ground into its build since its draft PR #246; this build is from before it, or didn't convert a WC3 map)", W3E_NAME);
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
    w->npoint = w->g->nground < POINT_SETS ? w->g->nground : POINT_SETS;
    uint8_t extended[POINT_SETS] = { 0 };
    for (int k = 0; k < w->npoint && !rc; ++k, ++w->nsets) {
        rc = load_tileset(r, chain, sheet, w->g->ground_ids + 4 * k, NULL, &w->sets[k], why);
        extended[k] = (uint8_t)w->sets[k].t.extended;
    }
    /* the cliffs (079), when the cliff sheet is somewhere: each cliff
     * tileset's ground, added when the map's list lacks it */
    for (int k = 0; k < 16; ++k) w->cliff_ground[k] = -1;
    if (!rc) {
        rc = wc3_cliffs_open(r, chain, w->g, &w->cliffs, &inner);
        if (rc == W_ERR_NOT_FOUND) rc = W_OK;            /* no cliffs: their cells laid as before */
        else if (rc) rc = w_fail(why, rc, "the cliffs: %s", inner.text);
    }
    for (int k = 0; w->cliffs && !rc && k < wc3_cliffs_types(w->cliffs); ++k) {
        const char *ground = wc3_cliffs_type(w->cliffs, k)->ground;
        if (!ground[0]) continue;
        int t = 0;
        while (t < w->nsets && memcmp(w->sets[t].t.id, ground, 4)) ++t;
        if (t == w->nsets) {
            if (w->nsets == TILESETS_MAX) continue;
            rc = load_tileset(r, chain, sheet, ground, NULL, &w->sets[t], why);
            if (rc) break;
            ++w->nsets;
        }
        w->cliff_ground[k] = t;
    }
    slk_free(sheet);
    /* the blight: the main tileset's blight picture, as the editor's data names it */
    w->blight = -1;
    char blight[128];
    if (!rc && w->nsets < TILESETS_MAX) {
        rc = blight_path(r, chain, w->g->tileset_code, blight, sizeof blight, why);
        if (rc == W_ERR_NOT_FOUND) rc = W_OK;            /* none named: blighted cells laid as before */
        else if (!rc) {
            rc = load_tileset(r, chain, NULL, WC3_BLIGHT_ID, blight, &w->sets[w->nsets], why);
            if (!rc) w->blight = w->nsets++;
        }
    }
    if (!rc) {
        rc = w3e_prep(w->g, extended, w->npoint, &w->prep, &inner);
        if (rc) rc = w_fail(why, rc, "%s: %s", W3E_NAME, inner.text);
    }
    if (rc) { wc3_ground_free(w); return rc; }
    *out = w;
    return W_OK;
}
/* }}} */

/* {{{ wc3_ground_tilesets, wc3_ground_tileset, wc3_ground_w3e, wc3_ground_cliffs */
int wc3_ground_tilesets(const Wc3Ground *w) { return w->nsets; }
const Wc3Tileset *wc3_ground_tileset(const Wc3Ground *w, int k) { return k >= 0 && k < w->nsets ? &w->sets[k].t : NULL; }
const W3eGround *wc3_ground_w3e(const Wc3Ground *w) { return w->g; }
const Wc3Cliffs *wc3_ground_cliffs(const Wc3Ground *w) { return w->cliffs; }
int wc3_ground_blight(const Wc3Ground *w) { return w->blight; }
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
        int right = !strcmp(ts->t.layer_right, name) && ts->t.layer_right[0];
        if (!right && strcmp(ts->t.layer, name)) continue;
        int sq = ts->t.height < ts->t.width ? ts->t.height : ts->t.width, x0 = right ? sq : 0;   /* the left square, or the right */
        unsigned char *o = malloc((size_t)sq * sq * 4);
        for (int y = 0; y < sq; ++y) memcpy(o + (size_t)y * sq * 4, ts->rgba + ((size_t)y * ts->t.width + x0) * 4, (size_t)sq * 4);
        *rgba = o; *side = sq;
        return W_OK;
    }
    return w_fail(why, W_ERR_NOT_FOUND, "no WC3 tileset's layer is named %s", name);
}
/* }}} */

/* {{{ wc3_ground_lay_tile */
/* the whole piece tileset t is laid with in cell (i, j): its south-west
 * point's variation (063) */
static uint8_t whole(const Wc3Ground *w, int t, int i, int j)
{
    return (uint8_t)w3e_variation(w->sets[t].t.extended, w3e_point(w->g, i, j)->texture_details);
}

/* a cell's (tileset, subtile) pairs, lowest first: 063's; for a cliff
 * cell with a model (not a ramp's), its cliff tileset's ground whole;
 * for a blighted cell, when the blight is named, its corners' tilesets
 * with the blight for each blighted one; for another bare (cliff or
 * blighted) cell its south-west point's tileset whole. The lowest's
 * piece is always its variation's. */
typedef struct { int n; uint8_t t[4], s[4]; int inside, bare, cliff, blight; } Cell;
static Cell cell_pairs(const Wc3Ground *w, int i, int j)
{
    Cell c = { 0 };
    c.inside = 1;
    int cw = w->g->width - 1;
    const uint8_t *e = w->prep->cells + ((size_t)j * cw + i) * 8;
    int cl = w->cliffs ? wc3_cliffs_at(w->cliffs, i, j) : -1;
    if (cl >= 0 && !wc3_cliffs_get(w->cliffs, cl)->ramp) {   /* a ramp's cell keeps its ground */
        int t = w->cliff_ground[wc3_cliffs_get(w->cliffs, cl)->type];
        if (t >= 0) { c.cliff = 1; c.t[0] = (uint8_t)t; c.s[0] = whole(w, t, i, j); c.n = 1; return c; }
    }
    if (w->blight >= 0 && w->prep->cell_kind[(size_t)j * cw + i] == W3E_CELL_BLIGHT) {
        /* each corner the blight where blighted, its tileset where not, laid as 063 lays tilesets */
        const W3ePoint *p[4] = { w3e_point(w->g, i, j), w3e_point(w->g, i + 1, j), w3e_point(w->g, i, j + 1), w3e_point(w->g, i + 1, j + 1) };
        static const uint8_t BIT[4] = { 2, 1, 8, 4 };    /* south-west, south-east, north-west, north-east */
        int t[4];
        for (int q = 0; q < 4; ++q) t[q] = p[q]->is_blight ? w->blight : p[q]->ground_texture < w->npoint ? p[q]->ground_texture : -1;
        c.blight = 1;
        for (int q = 0; q < 4; ++q) {
            if (t[q] < 0) continue;
            int seen = 0;
            for (int k = 0; k < c.n; ++k) seen |= c.t[k] == t[q];
            if (!seen) c.t[c.n++] = (uint8_t)t[q];
        }
        for (int a = 1; a < c.n; ++a)
            for (int b = a; b > 0 && c.t[b - 1] > c.t[b]; --b) { uint8_t s = c.t[b]; c.t[b] = c.t[b - 1]; c.t[b - 1] = s; }
        for (int k = 0; k < c.n; ++k) {
            int bits = 0;
            for (int q = 0; q < 4; ++q) if (t[q] == c.t[k]) bits |= BIT[q];
            c.s[k] = k ? (uint8_t)bits : whole(w, c.t[0], i, j);
        }
        return c;
    }
    if (w->prep->cell_kind[(size_t)j * cw + i] != W3E_CELL_TEXTURED) {
        c.bare = 1;
        int t = w3e_point(w->g, i, j)->ground_texture;
        if (t < w->npoint) { c.t[0] = (uint8_t)t; c.s[0] = whole(w, t, i, j); c.n = 1; }
        return c;
    }
    for (int q = 0; q < 4 && e[q * 2] != W3E_NONE; ++q)
        if (e[q * 2] < w->npoint) { c.t[c.n] = e[q * 2]; c.s[c.n] = e[q * 2 + 1]; ++c.n; }
    return c;
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

/* tileset t's piece s (0 to 31) as a picture of the tile and a subtile
 * in it: 16 and up in an extended picture's right half (one wider than
 * high but not twice: its left square's, as WC3 has none such); -1
 * when the tile's list is full */
static int piece(const Wc3Ground *w, AdtTile *adt, int t, int s, uint8_t *pic, uint8_t *sub)
{
    const Wc3Tileset *ts = &w->sets[t].t;
    int right = s >= 16 && ts->layer_right[0];
    int at = picture_of(adt, right ? ts->layer_right : ts->layer);
    if (at < 0) return -1;
    *pic = (uint8_t)at;
    *sub = (uint8_t)(s & 15);
    return 0;
}

WErr wc3_ground_lay_tile(const Wc3Ground *w, AdtTile *adt, Wc3Lay *lay, int chunk_tex[256][4], unsigned char painted[256], Wc3Paint *counts, WWhy *why)
{
    Wc3Paint none = { 0 };
    if (!counts) counts = &none;
    const float step = ADT_CHUNK_SIZE / 8;
    for (size_t c = 0; c < (size_t)WC3_TILE_CELLS * WC3_TILE_CELLS; ++c) { memset(lay[c].pic, 0, 4); memset(lay[c].sub, WC3_PIECE_NONE, 4); }
    for (int k = 0; k < 256; ++k) {
        painted[k] = 0;
        const AdtChunk *c = &adt->chunks[k];
        int crow = k / 16, ccol = k % 16;
        /* the chunk's 64 cells (r south, cc east), each by its centre */
        Cell cells[8][8];
        int use[TILESETS_MAX] = { 0 }, inside = 0, most = -1, nused = 0;
        for (int r = 0; r < 8; ++r)
            for (int cc = 0; cc < 8; ++cc) {
                int i, j;
                double x = c->x - (r + 0.5) * step, y = c->y - (cc + 0.5) * step;
                if (!wc3_ground_cell_at(w, x, y, &i, &j, NULL, NULL)) { memset(&cells[r][cc], 0, sizeof cells[r][cc]); continue; }
                cells[r][cc] = cell_pairs(w, i, j);
                ++inside;
                for (int q = 0; q < cells[r][cc].n; ++q) nused += !use[cells[r][cc].t[q]]++;
            }
        if (!inside) continue;                            /* off the WC3 grid: the converter's own pictures */
        for (int t = 0; t < w->nsets; ++t) if (use[t] && (most < 0 || use[t] > use[most])) most = t;
        if (most < 0) continue;                           /* every cell inside bare with no tileset named: left as it was */
        uint8_t mpic, msub;
        if (piece(w, adt, most, 15, &mpic, &msub)) return w_fail(why, W_ERR_FULL, "the tile's %d pictures and its WC3 layers come to more than 256", adt->ntextures);
        for (int l = 0; l < 4; ++l) chunk_tex[k][l] = mpic;
        if (nused > counts->most) counts->most = nused;
        for (int r = 0; r < 8; ++r)
            for (int cc = 0; cc < 8; ++cc) {
                const Cell *e = &cells[r][cc];
                Wc3Lay *o = &lay[(size_t)(crow * 8 + r) * WC3_TILE_CELLS + (size_t)(ccol * 8 + cc)];
                if (!e->inside || !e->n) {                /* outside the grid, or bare with no tileset: the chunk's most used, whole */
                    counts->outside += !e->inside;
                    counts->cells += e->inside;
                    counts->bare += e->bare;
                    o->pic[0] = mpic; o->sub[0] = msub;
                    continue;
                }
                ++counts->cells;
                counts->bare += e->bare;
                counts->cliff_ground += e->cliff;
                counts->blight += e->blight;
                counts->varied += e->s[0] != 15;
                for (int q = 0; q < e->n; ++q)
                    if (piece(w, adt, e->t[q], e->s[q], &o->pic[q], &o->sub[q]))
                        return w_fail(why, W_ERR_FULL, "the tile's %d pictures and its WC3 layers come to more than 256", adt->ntextures);
            }
        painted[k] = 1;
        ++counts->painted;
    }
    return W_OK;
}
/* }}} */

/* {{{ wc3_ground_place_cliffs */
static int model_index(AdtTile *adt, const char *name)
{
    for (int m = 0; m < adt->nmodels; ++m) if (!strcmp(adt->models[m], name)) return m;
    char (*grown)[260] = realloc(adt->models, sizeof *adt->models * (size_t)(adt->nmodels + 1));
    if (!grown) return -1;
    adt->models = grown;
    snprintf(adt->models[adt->nmodels], sizeof adt->models[adt->nmodels], "%s", name);
    return adt->nmodels++;
}

WErr wc3_ground_place_cliffs(const Wc3Ground *w, AdtTile *adt, int *placed, WWhy *why)
{
    if (placed) *placed = 0;
    if (!w->cliffs) return W_OK;
    /* the tile's square: its north-west chunk's corner, a tile's side south and east */
    const double side = ADT_CHUNK_SIZE * 16;
    double north = adt->chunks[0].x, west = adt->chunks[0].y;
    int cw = w->g->width - 1, n = wc3_cliffs_count(w->cliffs);
    for (int k = 0; k < n; ++k) {
        const Wc3Cliff *cl = wc3_cliffs_get(w->cliffs, k);
        /* its cell's centre decides the tile it is on, so each is placed once */
        double cx = w->g->offset_x + (cl->i + 0.5) * 128.0, cy = w->g->offset_y + (cl->j + 0.5) * 128.0;
        double x = w->x0 + w->scale * cy, y = w->y0 - w->scale * cx;
        if (!(x <= north && x > north - side && y <= west && y > west - side)) continue;
        int m = model_index(adt, wc3_cliffs_model(w->cliffs, cl->model));
        AdtPlace *grown = m < 0 ? NULL : realloc(adt->doodads, sizeof *adt->doodads * (size_t)(adt->ndoodads + 1));
        if (!grown) return w_fail(why, W_ERR_FULL, "no room for the tile's cliffs (%d placed)", adt->ndoodads);
        adt->doodads = grown;
        AdtPlace *p = &adt->doodads[adt->ndoodads++];
        memset(p, 0, sizeof *p);
        p->name = m;
        p->uid = WC3_CLIFF_UID + (uint32_t)(cl->j * cw + cl->i);
        p->x = (float)(w->x0 + w->scale * cl->y);        /* map.lua's rule: its origin, in yards */
        p->y = (float)(w->y0 - w->scale * cl->x);
        p->z = (float)(w->scale * cl->z);
        p->rot[1] = 90;                                  /* WC3's facing 0, as the converter turns its doodads */
        p->scale = 1;
        if (placed) ++*placed;
    }
    return W_OK;
}
/* }}} */
