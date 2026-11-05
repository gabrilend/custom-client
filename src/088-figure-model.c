/*
 * 088-figure-model.c (issues 173t, 1109) - which files a figure is made
 * from; see 088-figure-model.h
 *
 * For a general audience: the finding half of 041-figure.c's creature
 * building, moved here so it runs with no window, and through the
 * resolver for every file (it read the game's models from the archives
 * alone).
 */
#include "088-figure-model.h"
#include "016-dbc.h"
#include "085-baseline-set.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

void figure_model_free(FigureModel *m)
{
    asset_answer_free(&m->model);
    asset_answer_free(&m->skin);
    memset(m, 0, sizeof *m);
}

/* the stand-in's files into `m` (its other fields as they are) */
static WErr stand_in(Resolver *assets, MpqChain *chain, FigureModel *m, WWhy *why)
{
    WWhy inner;
    WErr rc = assets ? asset_read(assets, chain, BASELINE_FIGURE_MODEL, ASSET_MODEL, &m->model, &inner) : W_ERR_NOT_FOUND;
    if (!rc) rc = asset_read(assets, chain, BASELINE_FIGURE_SKIN, ASSET_MODEL, &m->skin, &inner);
    if (rc) {
        asset_answer_free(&m->model);
        return w_fail(why, rc, "no stand-in figure (%s): %s", BASELINE_FIGURE_MODEL, assets ? inner.text : "no resolver, so no baseline set");
    }
    m->kind = FIGURE_MODEL_STAND_IN;
    snprintf(m->base, sizeof m->base, "%s", BASELINE_FIGURE_MODEL);
    char *dot = strrchr(m->base, '.');
    if (dot) *dot = '\0';
    return W_OK;
}

WErr figure_model_stand_in(Resolver *assets, MpqChain *chain, const char *why_text, FigureModel *out, WWhy *why)
{
    memset(out, 0, sizeof *out);
    out->scale = 1;
    snprintf(out->stood_in_for, sizeof out->stood_in_for, "%s", why_text ? why_text : "");
    return stand_in(assets, chain, out, why);
}

/* a table's row by number, its failure said naming the display that asked */
static WErr row(Tables *tables, int display, const char *table, uint32_t id, TableRow *r, WWhy *why)
{
    WWhy inner;
    WErr rc = tables_row(tables, table, id, r, &inner);
    return rc ? w_fail(why, rc, "creature display %d: %s", display, inner.text) : W_OK;
}

WErr figure_model_for_display(Resolver *assets, MpqChain *chain, Tables *tables, int display, FigureModel *out, WWhy *why)
{
    memset(out, 0, sizeof *out);
    out->display = display;
    TableRow di, md;
    WErr rc;
    if ((rc = row(tables, display, "CreatureDisplayInfo", (uint32_t)display, &di, why))) return rc;
    uint32_t model_id = dbc_u32(di.dbc, di.row, 2), scale_bits = dbc_u32(di.dbc, di.row, 5);
    memcpy(&out->scale, &scale_bits, 4);
    if (!(out->scale > 0.01f && out->scale < 100)) out->scale = 1;
    out->extra = dbc_u32(di.dbc, di.row, 4);
    if (out->extra) { out->kind = FIGURE_MODEL_HUMANOID; return W_OK; }   /* a character look (173t): 025 builds it */
    for (int k = 0; k < 3; ++k) snprintf(out->skins[k], sizeof out->skins[k], "%s", dbc_text(di.dbc, di.row, 7 + (uint32_t)k));
    if ((rc = row(tables, display, "CreatureModelData", model_id, &md, why))) return rc;
    snprintf(out->named, sizeof out->named, "%s", dbc_text(md.dbc, md.row, 3));
    if (!out->named[0]) return w_fail(why, W_ERR_NOT_FOUND, "display %d names model %u, whose CreatureModelData row has no path", display, model_id);
    char base[260];
    snprintf(base, sizeof base, "%s", out->named);
    char *dot = strrchr(base, '.');
    if (dot) *dot = '\0';
    snprintf(out->folder, sizeof out->folder, "%s", base);
    char *cut = strrchr(out->folder, '\\');
    if (cut) cut[1] = '\0'; else out->folder[0] = '\0';

    /* a .mdx named as it is: a WC3 model a converted map brought (W02c
     * names a map's own model so; the game's own rows say .mdx too, but
     * the archives hold only the .m2). Asked for as named, with no
     * placeholder: a WC3 model (MDLX) found is the answer; anything else,
     * the game's .m2 beside the name, as the game's rows mean it. */
    size_t nlen = strlen(out->named);
    if (nlen > 4 && !strcasecmp(out->named + nlen - 4, ".mdx")) {
        WWhy inner;
        if (asset_read(assets, chain, out->named, ASSET_MODEL, &out->model, &inner) == W_OK) {
            if (out->model.len >= 4 && !memcmp(out->model.data, "MDLX", 4)) { out->kind = FIGURE_MODEL_WC3; return W_OK; }
            asset_answer_free(&out->model);
        }
    }
    /* the game's model: its .m2 and first view, from the first place that has them */
    char m2[300], skin[300];
    snprintf(m2, sizeof m2, "%s.m2", base);
    snprintf(skin, sizeof skin, "%s00.skin", base);
    WWhy inner;
    rc = asset_read(assets, chain, m2, ASSET_MODEL, &out->model, &inner);
    const char *missing = rc ? m2 : NULL;
    if (!rc) {
        rc = asset_read(assets, chain, skin, ASSET_MODEL, &out->skin, &inner);
        if (rc) { missing = skin; asset_answer_free(&out->model); }
    }
    if (!rc && (out->model.source == ASSET_FROM_BASELINE || out->skin.source == ASSET_FROM_BASELINE)) {
        /* the set answers every model the converter named with its post,
         * which has no skeleton for a creature to stand and walk with:
         * its stand-in figure stands in instead */
        asset_answer_free(&out->model);
        asset_answer_free(&out->skin);
        snprintf(out->stood_in_for, sizeof out->stood_in_for, "display %d's model %.180s: only the baseline set's post answers it", display, out->named);
        WWhy none;
        if (stand_in(assets, chain, out, &none) == W_OK) return W_OK;
        return w_fail(why, W_ERR_NOT_FOUND, "display %d (%s): only the baseline set's post answers it, and the stand-in figure isn't there: %s", display, out->named, none.text);
    }
    if (!rc) { out->kind = FIGURE_MODEL_GAME; snprintf(out->base, sizeof out->base, "%s", base); return W_OK; }
    if (rc != W_ERR_NOT_FOUND) return w_fail(why, rc, "display %d: %s", display, inner.text);
    /* in no place: the baseline set's stand-in, when a resolver is open and the set has it */
    snprintf(out->stood_in_for, sizeof out->stood_in_for, "display %d's model %.180s: %.100s is in no place", display, out->named, missing);
    WWhy none;
    if (assets && stand_in(assets, chain, out, &none) == W_OK) return W_OK;
    return w_fail(why, W_ERR_NOT_FOUND, "display %d (%s): its %s is in none of the %s%s", display, out->named, missing == m2 ? ".m2" : "00.skin",
                  assets ? "places, and the stand-in figure isn't either: " : "client's archives", assets ? none.text : "");
}
