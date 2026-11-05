/*
 * 081-wc3-water.c (issues 302, 1109) - a converted map's water as
 * Warcraft III draws it; see 081-wc3-water.h
 *
 * For a general audience: at the start the water sheet's row for the
 * map's tileset is read, every frame of its ripples found and decoded,
 * and every tile point's depth looked at once (the counts). After that
 * the handle is only read.
 */
#include "081-wc3-water.h"
#include "017-blp.h"
#include "077-slk.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SHEET "TerrainArt\\Water.slk"

/* {{{ the handle */
struct Wc3Water {
    const W3eGround *g;
    Wc3WaterKind     kind;
    Wc3WaterFrame    frames[WC3_WATER_FRAMES_MAX];
    Wc3WaterCounts   counts;
};
/* }}} */

/* {{{ static number: a cell's whole or decimal number, 1 when read */
static int number(const char *s, double *out)
{
    if (!s || !*s) return 0;
    char *end;
    errno = 0;
    double v = strtod(s, &end);
    while (*end == ' ') ++end;
    if (end == s || *end || errno || !isfinite(v)) return 0;
    *out = v;
    return 1;
}
/* }}} */

/* {{{ static colour: the four columns <prefix>A, R, G, B into RGBA */
static WErr colour(const Slk *sheet, const char *row, const char *prefix, uint8_t out[4], WWhy *why)
{
    static const char CH[4] = { 'R', 'G', 'B', 'A' };
    for (int c = 0; c < 4; ++c) {
        char col[16];
        snprintf(col, sizeof col, "%s%c", prefix, CH[c]);
        double v;
        const char *s = slk_value_any_case(sheet, row, col);
        if (!number(s, &v) || v < 0 || v > 255 || v != floor(v))
            return w_fail(why, W_ERR_CORRUPT, "%s: row %s's %s is %s, not a whole number from 0 to 255", SHEET, row, col, s ? s : "missing");
        out[c] = (uint8_t)v;
    }
    return W_OK;
}
/* }}} */

/* {{{ wc3_water_open, wc3_water_free */
void wc3_water_free(Wc3Water *w)
{
    if (!w) return;
    for (int k = 0; k < WC3_WATER_FRAMES_MAX; ++k) free(w->frames[k].rgba);
    free(w);
}

WErr wc3_water_open(Resolver *r, MpqChain *chain, const W3eGround *g, Wc3Water **out, WWhy *why)
{
    *out = NULL;
    if (!r) return w_fail(why, W_ERR_NOT_FOUND, "no resolver, so no converted map's WC3 water (a test-map run's)");
    WWhy inner;
    AssetAnswer a;
    WErr rc = asset_read(r, chain, SHEET, ASSET_TABLE, &a, &inner);
    if (rc == W_ERR_NOT_FOUND) return w_fail(why, rc, "no %s in any place (no Warcraft III install: the assetWc3 setting), so the converter's water stays", SHEET);
    if (rc) return w_fail(why, rc, "%s: %s", SHEET, inner.text);
    Slk *sheet;
    rc = slk_read(a.data, a.len, &sheet, &inner);
    asset_answer_free(&a);
    if (rc) return w_fail(why, rc, "%s: %s", SHEET, inner.text);

    Wc3Water *w = calloc(1, sizeof *w);
    w->g = g;
    Wc3WaterKind *k = &w->kind;
    snprintf(k->row, sizeof k->row, "%cSha", g->tileset_code);
    const char *file = slk_value_any_case(sheet, k->row, "texFile");
    double v;
    if (!file && !slk_value_any_case(sheet, k->row, "numTex")) {
        rc = w_fail(why, W_ERR_NOT_FOUND, "%s has no row %s (the map's tileset %c), so the converter's water stays", SHEET, k->row, g->tileset_code);
        goto fail;
    }
    if (!file || !*file || strlen(file) >= sizeof k->tex_file) { rc = w_fail(why, W_ERR_CORRUPT, "%s: row %s's texFile is %s", SHEET, k->row, file && *file ? "too long" : "empty"); goto fail; }
    snprintf(k->tex_file, sizeof k->tex_file, "%s", file);
    if (!number(slk_value_any_case(sheet, k->row, "numTex"), &v) || v < 1 || v > WC3_WATER_FRAMES_MAX || v != floor(v)) {
        rc = w_fail(why, W_ERR_CORRUPT, "%s: row %s's numTex isn't a whole number from 1 to %d", SHEET, k->row, WC3_WATER_FRAMES_MAX);
        goto fail;
    }
    k->frames = (int)v;
    k->rate = number(slk_value_any_case(sheet, k->row, "texRate"), &v) && v > 0 ? v : 1;
    k->height = number(slk_value_any_case(sheet, k->row, "height"), &v) ? v : 0;
    if ((rc = colour(sheet, k->row, "Smin_", k->shallow_min, why)) || (rc = colour(sheet, k->row, "Smax_", k->shallow_max, why))
        || (rc = colour(sheet, k->row, "Dmin_", k->deep_min, why)) || (rc = colour(sheet, k->row, "Dmax_", k->deep_max, why)))
        goto fail;
    slk_free(sheet);
    sheet = NULL;

    /* the frames, each through the resolver, never the checker */
    for (int f = 0; f < k->frames; ++f) {
        Wc3WaterFrame *fr = &w->frames[f];
        snprintf(fr->path, sizeof fr->path, "%s%02d.blp", k->tex_file, f);
        rc = asset_read(r, chain, fr->path, ASSET_TEXTURE, &a, &inner);
        if (rc == W_ERR_NOT_FOUND) { rc = w_fail(why, rc, "the water's frame %s is in no place, so the converter's water stays", fr->path); goto fail; }
        if (rc) { rc = w_fail(why, rc, "%s: %s", fr->path, inner.text); goto fail; }
        BlpImage img;
        rc = blp_decode(a.data, a.len, &img, &inner);
        fr->source = a.source;
        asset_answer_free(&a);
        if (rc) { rc = w_fail(why, W_ERR_CORRUPT, "the water's frame %s: %s", fr->path, inner.text); goto fail; }
        fr->width = img.width; fr->height = img.height; fr->rgba = img.rgba;   /* kept: blp_free would only free this */
    }

    /* the points' depths, once */
    for (int j = 0; j < g->height; ++j)
        for (int i = 0; i < g->width; ++i) {
            double d;
            if (!wc3_water_point(w, i, j, &d, NULL)) continue;
            ++w->counts.wet;
            if (d <= WC3_WATER_DEEP_LEVEL) ++w->counts.shallow; else ++w->counts.deep;
            if (d > w->counts.deepest) w->counts.deepest = d;
        }
    *out = w;
    return W_OK;
fail:
    slk_free(sheet);
    wc3_water_free(w);
    return rc;
}
/* }}} */

/* {{{ the answers */
const Wc3WaterKind *wc3_water_kind(const Wc3Water *w) { return &w->kind; }

const Wc3WaterFrame *wc3_water_frame(const Wc3Water *w, int k)
{
    return k >= 0 && k < w->kind.frames ? &w->frames[k] : NULL;
}

Wc3WaterCounts wc3_water_counts(const Wc3Water *w) { return w->counts; }

int wc3_water_frame_at(const Wc3Water *w, double seconds)
{
    if (!(seconds > 0)) return 0;
    return (int)fmod(floor(seconds * w->kind.rate), w->kind.frames);
}

void wc3_water_colour(const Wc3WaterKind *k, double depth, uint8_t out[4])
{
    const uint8_t *lo, *hi;
    double f;
    if (depth <= WC3_WATER_DEEP_LEVEL) {
        lo = k->shallow_min; hi = k->shallow_max;
        f = (depth > WC3_WATER_MIN_DEPTH ? depth - WC3_WATER_MIN_DEPTH : 0) / (WC3_WATER_DEEP_LEVEL - WC3_WATER_MIN_DEPTH);
    } else {
        lo = k->deep_min; hi = k->deep_max;
        f = depth - WC3_WATER_DEEP_LEVEL;
        if (f > WC3_WATER_MAX_DEPTH - WC3_WATER_DEEP_LEVEL) f = WC3_WATER_MAX_DEPTH - WC3_WATER_DEEP_LEVEL;
        f /= WC3_WATER_MAX_DEPTH - WC3_WATER_DEEP_LEVEL;
    }
    for (int c = 0; c < 4; ++c) out[c] = (uint8_t)lround(lo[c] + (hi[c] - lo[c]) * f);
}

int wc3_water_point(const Wc3Water *w, int i, int j, double *depth, uint8_t rgba[4])
{
    if (i < 0 || j < 0 || i >= w->g->width || j >= w->g->height) return 0;
    const W3ePoint *p = w3e_point(w->g, i, j);
    if (!w3e_is_wet(p)) return 0;
    double d = (w3e_water_z(p) - w3e_ground_z(p)) / 128;
    if (depth) *depth = d;
    if (rgba) wc3_water_colour(&w->kind, d, rgba);
    return 1;
}
/* }}} */
