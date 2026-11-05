/*
 * 076-placed-models.c (issues 1109 step 1c, 105a, 401, 204) - a placed
 * model's and a building's files, from wherever they are; see
 * 076-placed-models.h
 *
 * For a general audience: asks the resolver for a placed name's files in
 * the order its header gives (the name as written when it may be a WC3
 * model, else the game's .m2 and its view), and reads what comes back.
 */
#include "076-placed-models.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define WC3_YARDS_DEFAULT (1.0 / 30.72)   /* WC3's 128 units a cell to the game's 533.33 / 128 yards */

/* {{{ static ends_with: `name` ends in `tail`, without regard to case */
static int ends_with(const char *name, const char *tail)
{
    size_t n = strlen(name), t = strlen(tail);
    return n >= t && !strcasecmp(name + n - t, tail);
}
/* }}} */

/* {{{ static base_of: `name` without its ending (the last dot after the last slash), into out */
static WErr base_of(const char *name, char *out, size_t n, WWhy *why)
{
    if (!name || !name[0]) return w_fail(why, W_ERR_REFUSED, "a placed model with an empty name");
    size_t len = strlen(name);
    if (len + 12 > n) return w_fail(why, W_ERR_REFUSED, "a placed model's name is %zu characters, more than %zu: %.80s...", len, n - 12, name);
    memcpy(out, name, len + 1);
    char *dot = strrchr(out, '.');
    const char *slash = strrchr(out, '\\'), *fwd = strrchr(out, '/');
    if (fwd > slash) slash = fwd;
    if (dot && (!slash || dot > slash)) *dot = '\0';
    return W_OK;
}
/* }}} */

/* {{{ placed_model_drawn, placed_model_free */
const M2Model *placed_model_drawn(const PlacedModel *m) { return m->wc3 ? &m->mdx.model : &m->m2; }

void placed_model_free(PlacedModel *m)
{
    if (!m) return;
    if (m->wc3) mdx_free(&m->mdx);
    else m2_free(&m->m2);
    free(m->m2_bytes);
    memset(m, 0, sizeof *m);
}
/* }}} */

/* {{{ static from_m2: an .m2 and its view, read; both answers taken (freed here) */
static WErr from_m2(const char *name, AssetAnswer *m2, AssetAnswer *skin, PlacedModel *out, WWhy *why)
{
    WWhy inner;
    WErr bad = m2_read(m2->data, m2->len, skin->data, skin->len, &out->m2, &inner);
    asset_answer_free(skin);
    if (bad) { asset_answer_free(m2); return w_fail(why, inner.code, "%s: %s", name, inner.text); }
    out->m2_bytes = m2->data;                            /* kept for the skeleton, particles and solid shape */
    out->m2_len = m2->len;
    out->source = m2->source;
    out->from = m2->from;
    m2->data = NULL;
    return W_OK;
}
/* }}} */

/* {{{ placed_model_read */
WErr placed_model_read(Resolver *r, MpqChain *chain, const char *name, double wc3_yards, PlacedModel *out, WWhy *why)
{
    memset(out, 0, sizeof *out);
    if (base_of(name, out->base, sizeof out->base, why)) return why->code;
    WWhy inner;

    /* a WC3 model the map brought: asked for as named, never a placeholder
     * (the game's own models are named .mdx too, and are .m2 files) */
    if (ends_with(name, ".mdx")) {
        AssetAnswer a;
        WErr rc = asset_read(r, chain, name, ASSET_MODEL, &a, &inner);
        if (rc && rc != W_ERR_NOT_FOUND) return w_fail(why, rc, "%s: %s", name, inner.text);
        if (!rc && a.len >= 4 && !memcmp(a.data, "MDLX", 4)) {
            WErr bad = mdx_read(a.data, a.len, &out->mdx, &inner);
            AssetSource source = a.source;
            const char *from = a.from;
            asset_answer_free(&a);
            if (bad) { memset(out, 0, sizeof *out); return w_fail(why, inner.code, "%s: %s", name, inner.text); }
            out->wc3 = 1;
            out->source = source;
            out->from = from;
            float yards = (float)(wc3_yards > 0 ? wc3_yards : WC3_YARDS_DEFAULT);
            M2Model *d = &out->mdx.model;
            for (uint32_t i = 0; i < d->nvertices; ++i) { d->vertices[i].x *= yards; d->vertices[i].y *= yards; d->vertices[i].z *= yards; }
            return W_OK;
        }
        if (!rc) asset_answer_free(&a);                  /* something else under that name: the .m2 beside it, as for any name */
    }

    /* the game's model: its .m2 and first view, each from the first place
     * that has it; the box, whole, when either is nowhere */
    char m2_name[280], skin_name[280];
    snprintf(m2_name, sizeof m2_name, "%s.m2", out->base);
    snprintf(skin_name, sizeof skin_name, "%s00.skin", out->base);
    AssetAnswer m2, skin;
    WErr rc = asset_get(r, chain, m2_name, ASSET_MODEL, &m2, &inner);
    if (rc) return w_fail(why, rc, "%s: %s", name, inner.text);
    if (m2.source == ASSET_FROM_PLACEHOLDER) rc = asset_stand_in(r, skin_name, ASSET_MODEL, &skin, &inner);
    else rc = asset_get(r, chain, skin_name, ASSET_MODEL, &skin, &inner);
    if (rc) { asset_answer_free(&m2); return w_fail(why, rc, "%s: %s", name, inner.text); }
    if (skin.source == ASSET_FROM_PLACEHOLDER && m2.source != ASSET_FROM_PLACEHOLDER) {
        asset_answer_free(&m2);                          /* its view is nowhere: the box's own .m2 with the box's view */
        rc = asset_stand_in(r, m2_name, ASSET_MODEL, &m2, &inner);
        if (rc) { asset_answer_free(&skin); return w_fail(why, rc, "%s: %s", name, inner.text); }
    }
    return from_m2(name, &m2, &skin, out, why);
}
/* }}} */

/* {{{ placed_model_stand_in */
WErr placed_model_stand_in(Resolver *r, const char *name, PlacedModel *out, WWhy *why)
{
    memset(out, 0, sizeof *out);
    if (base_of(name, out->base, sizeof out->base, why)) return why->code;
    if (!r) return w_fail(why, W_ERR_NOT_FOUND, "%s: no resolver, so no placeholder stands in", name);
    WWhy inner;
    char skin_name[280];
    snprintf(skin_name, sizeof skin_name, "%s00.skin", out->base);
    AssetAnswer m2, skin;
    WErr rc = asset_stand_in(r, name, ASSET_MODEL, &m2, &inner);
    if (rc) return w_fail(why, rc, "%s: %s", name, inner.text);
    rc = asset_stand_in(r, skin_name, ASSET_MODEL, &skin, &inner);
    if (rc) { asset_answer_free(&m2); return w_fail(why, rc, "%s: %s", name, inner.text); }
    return from_m2(name, &m2, &skin, out, why);
}
/* }}} */

/* {{{ placed_building_root, placed_building_group */
WErr placed_building_root(Resolver *r, MpqChain *chain, const char *name, WmoRoot *out, AssetSource *source, WWhy *why)
{
    memset(out, 0, sizeof *out);
    AssetAnswer a;
    WWhy inner;
    WErr rc = asset_read(r, chain, name, ASSET_MODEL, &a, &inner);
    if (rc) return w_fail(why, rc, "%s", inner.text);
    rc = wmo_read_root(a.data, a.len, out, &inner);
    if (source) *source = a.source;
    asset_answer_free(&a);
    return rc ? w_fail(why, rc, "%s: %s", name, inner.text) : W_OK;
}

WErr placed_building_group(Resolver *r, MpqChain *chain, const char *name, int group, WmoGroup *out, WWhy *why)
{
    memset(out, 0, sizeof *out);
    char base[260], path[300];
    if (base_of(name, base, sizeof base, why)) return why->code;
    if (group < 0 || group > 999) return w_fail(why, W_ERR_REFUSED, "%s: part %d, outside 0 to 999", name, group);
    snprintf(path, sizeof path, "%s_%03d.wmo", base, group);
    AssetAnswer a;
    WWhy inner;
    WErr rc = asset_read(r, chain, path, ASSET_MODEL, &a, &inner);
    if (rc) return w_fail(why, rc, "%s", inner.text);
    rc = wmo_read_group(a.data, a.len, out, &inner);
    asset_answer_free(&a);
    return rc ? w_fail(why, rc, "%s: %s", path, inner.text) : W_OK;
}
/* }}} */
