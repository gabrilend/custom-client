/*
 * 079-wc3-cliffs.c (issues 302, 1109) - a converted map's cliffs as
 * Warcraft III builds them; see 079-wc3-cliffs.h
 *
 * For a general audience: at the start the cliff sheet is read and every
 * cell looked at once; each distinct model is asked for once (from the
 * variation asked down to 0) to learn which exist. After that the handle
 * is only read.
 */
#include "079-wc3-cliffs.h"
#include "076-placed-models.h"
#include "077-slk.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SHEET      "TerrainArt\\CliffTypes.slk"
#define TYPES_MAX  16                        /* a tile point names 0 to 14 (15: none; 063: four bits) */

/* {{{ the handle */
typedef struct { char path[200]; int found; } Asked;    /* a model path asked for, and whether some place has it */
struct Wc3Cliffs {
    const W3eGround *g;
    Wc3CliffType     types[TYPES_MAX];
    int              ntypes;
    Wc3Cliff        *cliffs;  int ncliffs;
    int             *at;                         /* per cell: into cliffs, or -1 */
    char           (*models)[400];  int nmodels, cap_models;
    Asked           *asked;   int nasked, cap_asked;
    Wc3CliffCounts   counts;
};
/* }}} */

/* {{{ wc3_cliff_letters */
int wc3_cliff_letters(int nw, int ne, int se, int sw, char out[5], int *steep)
{
    int level[4] = { nw, ne, se, sw }, lo = nw;
    for (int q = 1; q < 4; ++q) if (level[q] < lo) lo = level[q];
    if (steep) *steep = 0;
    for (int q = 0; q < 4; ++q) {
        int up = level[q] - lo;
        if (up > 2) { up = 2; if (steep) *steep = 1; }
        out[q] = (char)('A' + up);
    }
    out[4] = '\0';
    return lo;
}
/* }}} */

/* {{{ wc3_ramp_letters */
int wc3_ramp_letters(int nw, int ne, int se, int sw, int rnw, int rne, int rse, int rsw, char out[5], int *steep)
{
    int level[4] = { nw, ne, se, sw }, ramp[4] = { rnw, rne, rse, rsw }, lo = nw;
    for (int q = 1; q < 4; ++q) if (level[q] < lo) lo = level[q];
    if (steep) *steep = 0;
    for (int q = 0; q < 4; ++q) {
        int up = level[q] - lo;
        if (up > 2) { up = 2; if (steep) *steep = 1; }
        out[q] = (char)(ramp[q] ? 'L' - 4 * up : 'A' + up);
    }
    out[4] = '\0';
    return lo;
}
/* }}} */

/* {{{ static asked: whether some place has `path` (each path asked once) */
static WErr asked(Wc3Cliffs *c, Resolver *r, MpqChain *chain, const char *path, int *found, WWhy *why)
{
    for (int k = 0; k < c->nasked; ++k) if (!strcmp(c->asked[k].path, path)) { *found = c->asked[k].found; return W_OK; }
    AssetAnswer a;
    WWhy inner;
    WErr rc = asset_read(r, chain, path, ASSET_MODEL, &a, &inner);
    if (rc && rc != W_ERR_NOT_FOUND) return w_fail(why, rc, "%s: %s", path, inner.text);
    if (!rc) asset_answer_free(&a);
    if (c->nasked == c->cap_asked) { c->cap_asked = c->cap_asked ? c->cap_asked * 2 : 64; c->asked = realloc(c->asked, sizeof *c->asked * (size_t)c->cap_asked); }
    snprintf(c->asked[c->nasked].path, sizeof c->asked[c->nasked].path, "%s", path);
    c->asked[c->nasked++].found = *found = !rc;
    return W_OK;
}
/* }}} */

/* {{{ static model_of: the model's number for a name, added when new */
static int model_of(Wc3Cliffs *c, const char *name, int found)
{
    for (int m = 0; m < c->nmodels; ++m) if (!strcmp(c->models[m], name)) return m;
    if (c->nmodels == c->cap_models) { c->cap_models = c->cap_models ? c->cap_models * 2 : 32; c->models = realloc(c->models, sizeof *c->models * (size_t)c->cap_models); }
    snprintf(c->models[c->nmodels], sizeof c->models[c->nmodels], "%s", name);
    ++c->counts.models;
    c->counts.found += found;
    return c->nmodels++;
}
/* }}} */

/* {{{ static cell_type: a cliff cell's tileset (south-west first), or -1 */
static int cell_type(const Wc3Cliffs *c, const W3ePoint *p[4])
{
    for (int q = 0; q < 4; ++q) if (p[q]->cliff_texture < c->ntypes) return p[q]->cliff_texture;
    return -1;
}
/* }}} */

/* {{{ wc3_cliffs_open, wc3_cliffs_free */
void wc3_cliffs_free(Wc3Cliffs *c)
{
    if (!c) return;
    free(c->cliffs); free(c->at); free(c->models); free(c->asked);
    free(c);
}

WErr wc3_cliffs_open(Resolver *r, MpqChain *chain, const W3eGround *g, Wc3Cliffs **out, WWhy *why)
{
    *out = NULL;
    if (!r) return w_fail(why, W_ERR_NOT_FOUND, "no resolver, so no converted map's WC3 cliffs (a test-map run's)");
    WWhy inner;
    AssetAnswer a;
    WErr rc = asset_read(r, chain, SHEET, ASSET_TABLE, &a, &inner);
    if (rc == W_ERR_NOT_FOUND) return w_fail(why, rc, "no %s in any place (no Warcraft III install: the assetWc3 setting), so its cliffs' models aren't known", SHEET);
    if (rc) return w_fail(why, rc, "%s: %s", SHEET, inner.text);
    Slk *sheet;
    rc = slk_read(a.data, a.len, &sheet, &inner);
    asset_answer_free(&a);
    if (rc) return w_fail(why, rc, "%s: %s", SHEET, inner.text);

    Wc3Cliffs *c = calloc(1, sizeof *c);
    c->g = g;
    /* the map's cliff tilesets, by the sheet's rows */
    c->ntypes = g->ncliff < TYPES_MAX - 1 ? g->ncliff : TYPES_MAX - 1;
    for (int k = 0; k < c->ntypes; ++k) {
        Wc3CliffType *t = &c->types[k];
        memcpy(t->id, g->cliff_ids + 4 * k, 4);
        t->id[4] = '\0';
        const char *dir = slk_value_any_case(sheet, t->id, "cliffModelDir"), *rdir = slk_value_any_case(sheet, t->id, "rampModelDir");
        const char *tdir = slk_value_any_case(sheet, t->id, "texDir"), *tfile = slk_value_any_case(sheet, t->id, "texFile");
        const char *ground = slk_value_any_case(sheet, t->id, "groundTile");
        t->known = slk_value_any_case(sheet, t->id, "cliffID") || dir || tdir || ground;
        snprintf(t->model_dir, sizeof t->model_dir, "%s", dir && dir[0] ? dir : "Cliffs");
        snprintf(t->ramp_dir, sizeof t->ramp_dir, "%s", rdir && rdir[0] ? rdir : "CliffTrans");
        if (tdir && tfile && tfile[0]) snprintf(t->picture, sizeof t->picture, "%s\\%s.blp", tdir, tfile);
        else snprintf(t->picture, sizeof t->picture, "TerrainArt\\%s.blp", t->id);   /* nowhere: the checker */
        if (ground && strlen(ground) == 4) memcpy(t->ground, ground, 5);
    }
    slk_free(sheet);

    /* every cell on different levels: a cliff, or a ramp where a corner is one */
    int cw = g->width - 1, ch = g->height - 1;
    c->at = malloc(sizeof *c->at * (size_t)(cw > 0 && ch > 0 ? cw * ch : 1));
    rc = W_OK;
    for (int j = 0; j < ch && !rc; ++j)
        for (int i = 0; i < cw && !rc; ++i) {
            c->at[j * cw + i] = -1;
            const W3ePoint *sw = w3e_point(g, i, j), *se = w3e_point(g, i + 1, j), *nw = w3e_point(g, i, j + 1), *ne = w3e_point(g, i + 1, j + 1);
            const W3ePoint *p[4] = { sw, se, nw, ne };
            int lo = 99, hi = -1, ramp = 0;
            for (int q = 0; q < 4; ++q) {
                if (p[q]->layer_height < lo) lo = p[q]->layer_height;
                if (p[q]->layer_height > hi) hi = p[q]->layer_height;
                ramp |= p[q]->is_ramp;
            }
            if (hi == lo) continue;
            if (ramp) ++c->counts.ramps; else ++c->counts.cells;
            int type = cell_type(c, p);
            if (type < 0) { if (ramp) ++c->counts.ramps_no_tileset; else ++c->counts.no_tileset; continue; }
            char letters[5];
            int steep;
            int base = ramp ? wc3_ramp_letters(nw->layer_height, ne->layer_height, se->layer_height, sw->layer_height,
                                               nw->is_ramp, ne->is_ramp, se->is_ramp, sw->is_ramp, letters, &steep)
                            : wc3_cliff_letters(nw->layer_height, ne->layer_height, se->layer_height, sw->layer_height, letters, &steep);
            c->counts.steep += steep;
            /* its variation, or the highest below it some place has, else 0 */
            const Wc3CliffType *t = &c->types[type];
            const char *dir = ramp ? t->ramp_dir : t->model_dir;
            char path[200];
            int found = 0, v = sw->cliff_variation;
            for (; v >= 0 && !rc; --v) {
                snprintf(path, sizeof path, "Doodads\\Terrain\\%s\\%s%s%d.mdx", dir, dir, letters, v);
                rc = asked(c, r, chain, path, &found, why);
                if (found) break;
            }
            if (rc) break;
            if (!found) snprintf(path, sizeof path, "Doodads\\Terrain\\%s\\%s%s0.mdx", dir, dir, letters);
            else if (v < sw->cliff_variation) ++c->counts.fallback;
            char name[400];
            snprintf(name, sizeof name, "%s%s11=%s", path, PLACED_REPLACE_MARK, t->picture);
            int m = model_of(c, name, found);
            double mean = (sw->height + se->height + nw->height + ne->height) / 4.0;
            Wc3Cliff cl = { i, j, type, base, m, ramp,
                            g->offset_x + (i + 1) * 128.0, g->offset_y + j * 128.0,
                            (base - 2) * 128.0 + mean };
            if (c->ncliffs % 256 == 0) c->cliffs = realloc(c->cliffs, sizeof *c->cliffs * (size_t)(c->ncliffs + 256));
            c->at[j * cw + i] = c->ncliffs;
            c->cliffs[c->ncliffs++] = cl;
            if (ramp) ++c->counts.ramps_placed; else ++c->counts.placed;
        }
    if (rc) { wc3_cliffs_free(c); return rc; }
    *out = c;
    return W_OK;
}
/* }}} */

/* {{{ the readers */
int                 wc3_cliffs_types(const Wc3Cliffs *c) { return c->ntypes; }
const Wc3CliffType *wc3_cliffs_type(const Wc3Cliffs *c, int k) { return k >= 0 && k < c->ntypes ? &c->types[k] : NULL; }
int                 wc3_cliffs_count(const Wc3Cliffs *c) { return c->ncliffs; }
const Wc3Cliff     *wc3_cliffs_get(const Wc3Cliffs *c, int k) { return k >= 0 && k < c->ncliffs ? &c->cliffs[k] : NULL; }
void                wc3_cliffs_counts(const Wc3Cliffs *c, Wc3CliffCounts *out) { *out = c->counts; }
const char         *wc3_cliffs_model(const Wc3Cliffs *c, int m) { return m >= 0 && m < c->nmodels ? c->models[m] : NULL; }
int wc3_cliffs_at(const Wc3Cliffs *c, int i, int j)
{
    int cw = c->g->width - 1, ch = c->g->height - 1;
    return i >= 0 && i < cw && j >= 0 && j < ch ? c->at[j * cw + i] : -1;
}
/* }}} */
