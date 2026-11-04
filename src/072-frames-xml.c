/*
 * 072-frames-xml.c (issue 703) - see 072-frames-xml.h.
 *
 * The builder keeps a table from each XML object (066's FxmlObject, by
 * its address, which 066 keeps for its handle's life) to the tree object
 * made from it, so an anchor or parent naming an XML object finds what
 * it became, named or not.
 */
#include "072-frames-xml.h"
#include "071-frames.h"
#include "066-framexml.h"
#include "013-errors.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* {{{ the handle */
struct FramesXml {
    Frames *frames;
    FramesXmlOptions opt;
    const FxmlObject **keys;              /* open addressing: object -> id */
    int *vals;
    size_t cap, n;
    int next_step;                        /* steps of the Fxml built so far */
    int warnings;
};

FramesXml *frames_xml_new(Frames *frames, const FramesXmlOptions *opt, WWhy *why)
{
    FramesXml *x = calloc(1, sizeof *x);
    if (!x) { w_fail(why, W_ERR_FULL, "no memory to build the interface's XML into frames"); return NULL; }
    x->frames = frames;
    if (opt) x->opt = *opt;
    return x;
}

void frames_xml_free(FramesXml *x)
{
    if (!x) return;
    free(x->keys);
    free(x->vals);
    free(x);
}

static size_t slot_of(const FramesXml *x, const FxmlObject *o)
{
    uintptr_t h = (uintptr_t)o;
    h ^= h >> 17; h *= 0x9E3779B1u; h ^= h >> 13;
    size_t i = (size_t)h & (x->cap - 1);
    while (x->keys[i] && x->keys[i] != o) i = (i + 1) & (x->cap - 1);
    return i;
}

int frames_xml_id(const FramesXml *x, const FxmlObject *o)
{
    if (!o || !x->cap) return FRAMES_NONE;
    size_t i = slot_of(x, o);
    return x->keys[i] ? x->vals[i] : FRAMES_NONE;
}

static WErr remember(FramesXml *x, const FxmlObject *o, int id, WWhy *why)
{
    if ((x->n + 1) * 2 > x->cap) {
        FramesXml g = { .cap = x->cap ? x->cap * 2 : 256 };
        g.keys = calloc(g.cap, sizeof *g.keys);
        g.vals = malloc(g.cap * sizeof *g.vals);
        if (!g.keys || !g.vals) { free(g.keys); free(g.vals); return w_fail(why, W_ERR_FULL, "no memory to build the interface's XML into frames"); }
        for (size_t k = 0; k < x->cap; ++k)
            if (x->keys[k]) { size_t i = slot_of(&g, x->keys[k]); g.keys[i] = x->keys[k]; g.vals[i] = x->vals[k]; }
        free(x->keys); free(x->vals);
        x->keys = g.keys; x->vals = g.vals; x->cap = g.cap;
    }
    size_t i = slot_of(x, o);
    if (!x->keys[i]) ++x->n;
    x->keys[i] = o;
    x->vals[i] = id;
    return W_OK;
}

int frames_xml_warning_count(const FramesXml *x) { return x->warnings; }
/* }}} */

/* {{{ warnings: this client's own wording (docs/057 note 1.15 may give the original's) */
static void warn(FramesXml *x, const FxmlNode *at, const char *format, ...) __attribute__((format(printf, 3, 4)));
static void warn(FramesXml *x, const FxmlNode *at, const char *format, ...)
{
    char text[600];
    va_list ap;
    va_start(ap, format);
    vsnprintf(text, sizeof text, format, ap);
    va_end(ap);
    ++x->warnings;
    if (x->opt.warn) x->opt.warn(x->opt.ctx, at && at->file ? at->file->path : "(CreateFrame)", at ? at->line : 0, text);
}
static const char *who(const FxmlObject *o) { return o->name ? o->name : o->type; }
static int is_true(const char *v) { return v && !strcasecmp(v, "true"); }
/* the element that set an attribute, for the warning's line */
static const FxmlNode *setter(const FxmlObject *o, const char *name)
{
    for (int i = 0; i < o->nsets; ++i) if (!strcasecmp(o->sets[i].name, name)) return o->sets[i].from;
    return o->node;
}
/* }}} */

/* {{{ static object_of: an XML object or a name, as a tree id; FRAMES_NONE when not in the tree */
static int object_of(FramesXml *x, const FxmlObject *o, const char *name)
{
    int id = frames_xml_id(x, o);
    if (id == FRAMES_NONE && name) id = frames_find(x->frames, name);
    return id;
}
/* }}} */

/* {{{ static size_from: each <Size> written in one of the object's pieces, in load order */
static int in_pieces(const FxmlObject *o, const FxmlNode *n)
{
    for (int i = 0; i < o->npieces; ++i) if (o->pieces[i] == n) return 1;
    return 0;
}
static void size_from(FramesXml *x, const FxmlObject *o, int id)
{
    WWhy why;
    for (int i = 0; i < o->nparts; ++i) {
        const FxmlNode *s = o->parts[i];
        if (strcasecmp(s->tag, "Size") || !in_pieces(o, s->up)) continue;
        const char *w = fxml_attr(s, "x"), *h = fxml_attr(s, "y");
        for (int k = 0; k < s->nkids; ++k)
            if (!strcasecmp(s->kids[k]->tag, "AbsDimension")) { w = fxml_attr(s->kids[k], "x"); h = fxml_attr(s->kids[k], "y"); }
        if (w && frames_set_width(x->frames, id, strtod(w, NULL), &why)) warn(x, s, "%s: width %s: %s", who(o), w, why.text);
        if (h && frames_set_height(x->frames, id, strtod(h, NULL), &why)) warn(x, s, "%s: height %s: %s", who(o), h, why.text);
    }
}
/* }}} */

/* {{{ static place: anchors, setAllPoints, and a named part's default */
static void place(FramesXml *x, const FxmlObject *o, int id)
{
    WWhy why;
    for (int i = 0; i < o->nanchors; ++i) {
        const FxmlAnchor *a = &o->anchors[i];
        int rel = FRAMES_SCREEN;
        if (a->relative || a->relative_name) {
            rel = object_of(x, a->relative, a->relative_name);
            if (rel == FRAMES_NONE) {
                warn(x, a->from, "%s: %s isn't in the frame tree, anchor skipped", who(o), a->relative_name ? a->relative_name : "its unnamed relative");
                continue;
            }
        }
        if (frames_set_point(x->frames, id, frames_point_from_name(a->point), rel, frames_point_from_name(a->relative_point), a->x, a->y, &why))
            warn(x, a->from, "%s: anchor %s skipped: %s", who(o), a->point, why.text);
    }
    if (o->set_all_points) {
        if (frames_set_all_points(x->frames, id, FRAMES_NONE, &why)) warn(x, setter(o, "setAllPoints"), "%s: setAllPoints: %s", who(o), why.text);
    } else if (!o->nanchors && o->kind == FXML_REGION && o->slot && frames_parent(x->frames, id) != FRAMES_NONE) {
        if (!strcasecmp(o->slot, "ButtonText")) frames_set_point(x->frames, id, FRAMES_CENTER, FRAMES_NONE, FRAMES_CENTER, 0, 0, &why);
        else frames_set_all_points(x->frames, id, FRAMES_NONE, &why);
    }
}
/* }}} */

/* {{{ static frame_values: scale, strata, level, clamping */
static void frame_values(FramesXml *x, const FxmlObject *o, int id)
{
    WWhy why;
    const char *v;
    if ((v = fxml_set(o, "scale")) && frames_set_scale(x->frames, id, strtod(v, NULL), &why))
        warn(x, setter(o, "scale"), "%s: scale %s refused: %s", who(o), v, why.text);
    if ((v = fxml_set(o, "frameStrata"))) {
        FramesStrata s = frames_strata_from_name(v);
        if (s == FRAMES_STRATA_COUNT) warn(x, setter(o, "frameStrata"), "%s: %s isn't a frame strata, ignored", who(o), v);
        else frames_set_strata(x->frames, id, s, &why);
    }
    if ((v = fxml_set(o, "frameLevel"))) {
        char *end;
        long level = strtol(v, &end, 10);
        if (end == v) warn(x, setter(o, "frameLevel"), "%s: frameLevel %s isn't a number, ignored", who(o), v);
        else if (level < 0 || (level == 0 && !x->opt.level_zero))
            warn(x, setter(o, "frameLevel"), "%s: frameLevel %s refused, it must be above 0", who(o), v);   /* the quirk, unless corrected */
        else frames_set_level_direct(x->frames, id, level > FRAMES_MAX_LEVEL ? FRAMES_MAX_LEVEL : (int)level, &why);
    }
    if (is_true(fxml_set(o, "clampedToScreen"))) frames_set_clamped(x->frames, id, 1);
}
/* }}} */

/* {{{ static region_layer: a region's draw layer and sublevel from the XML */
static void region_layer(FramesXml *x, const FxmlObject *o, FramesLayer *layer, int *sublevel)
{
    *layer = FRAMES_LAYER_ARTWORK;
    *sublevel = 0;
    if (o->layer) {
        FramesLayer l = frames_layer_from_name(o->layer);
        if (l == FRAMES_LAYER_COUNT) warn(x, o->node->up, "%s: %s isn't a draw layer, ARTWORK used", who(o), o->layer);
        else *layer = l;
        const FxmlNode *in = o->node->up;
        const char *sub = in && !strcasecmp(in->tag, "Layer") ? fxml_attr(in, "textureSubLevel") : NULL;
        if (sub) {
            long s = strtol(sub, NULL, 10);
            if (s < FRAMES_SUBLEVEL_MIN || s > FRAMES_SUBLEVEL_MAX)
                warn(x, in, "%s: textureSubLevel %s is outside %d to %d, 0 used", who(o), sub, FRAMES_SUBLEVEL_MIN, FRAMES_SUBLEVEL_MAX);
            else *sublevel = (int)s;
        }
    } else if (o->slot && !strcasecmp(o->slot, "HighlightTexture")) *layer = FRAMES_LAYER_HIGHLIGHT;
}
/* }}} */

/* {{{ build */
static WErr build(FramesXml *x, const FxmlObject *o, int *out, WWhy *why)
{
    *out = FRAMES_NONE;
    int had = frames_xml_id(x, o);
    if (had != FRAMES_NONE) { *out = had; return W_OK; }
    if (o->kind == FXML_FONT) return W_OK;
    int parent = FRAMES_NONE;
    if (o->parent || o->parent_name) {
        parent = object_of(x, o->parent, o->parent_name);
        if (parent == FRAMES_NONE)
            warn(x, o->node, "%s: its parent %s isn't in the frame tree, so it has none", who(o), o->parent_name ? o->parent_name : "(unnamed)");
        else if (frames_is_region(x->frames, parent)) parent = FRAMES_NONE;
    }
    int id;
    if (o->kind == FXML_REGION) {
        if (parent == FRAMES_NONE) { warn(x, o->node, "%s: a region with no frame to go on, not made", who(o)); return W_OK; }
        FramesLayer layer;
        int sublevel;
        region_layer(x, o, &layer, &sublevel);
        id = frames_new_region(x->frames, o->type, o->name, parent, layer, sublevel, why);
    } else id = frames_new_frame(x->frames, o->type, o->name, parent, why);
    if (id == FRAMES_NONE) return why->code;
    WErr e = remember(x, o, id, why);
    if (e) return e;
    size_from(x, o, id);
    place(x, o, id);
    if (o->kind == FXML_FRAME) frame_values(x, o, id);
    if (is_true(fxml_set(o, "hidden"))) frames_hide(x->frames, id);
    for (int i = 0; i < o->nkids; ++i) {
        int kid;
        if ((e = build(x, o->kids[i], &kid, why))) return e;
    }
    *out = id;
    return W_OK;
}

WErr frames_xml_build(FramesXml *x, const FxmlObject *o, int *id, WWhy *why)
{
    int dummy;
    return build(x, o, id ? id : &dummy, why);
}

WErr frames_xml_catch_up(FramesXml *x, const Fxml *fx, WWhy *why)
{
    for (; x->next_step < fxml_step_count(fx); ++x->next_step) {
        const FxmlStep *s = fxml_step_at(fx, x->next_step);
        if (s->kind != FXML_STEP_OBJECT) continue;
        int id;
        WErr e = build(x, s->object, &id, why);
        if (e) return e;
    }
    return W_OK;
}
/* }}} */
