/*
 * 071-frames.c (issue 703) - see 071-frames.h.
 *
 * The objects are one array, indexed by id, never shrinking. Beside it
 * the handle keeps working arrays sized to the same count (the layout's
 * lists, the circle finder's stacks, the draw order's keys), grown when
 * an object is made, so laying out never asks for memory and can't fail.
 *
 * What depends on the parent chain (effective scale, strata, level,
 * visible) is worked out on demand and kept until anything in a chain
 * changes (`tree_gen`). Layout keeps a list of objects whose own anchors,
 * size or scale changed ("dirty"); a pass takes them and everything tied
 * to them, puts them in dependency order (an object once all it is tied
 * to is done), and computes each whose anchors' objects moved. What
 * never comes up in that order is waiting on a circle: those are left
 * unplaced, and the circle finder says which of them are in one.
 */
#include "071-frames.h"
#include "013-errors.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* {{{ the names, in each enum's order */
static const char *const POINT_NAMES[FRAMES_POINT_COUNT] = {
    "TOPLEFT", "TOP", "TOPRIGHT", "LEFT", "CENTER", "RIGHT", "BOTTOMLEFT", "BOTTOM", "BOTTOMRIGHT",
};
static const char *const STRATA_NAMES[FRAMES_STRATA_COUNT] = {
    "PARENT", "BACKGROUND", "LOW", "MEDIUM", "HIGH", "DIALOG", "FULLSCREEN", "FULLSCREEN_DIALOG", "TOOLTIP",
};
static const char *const LAYER_NAMES[FRAMES_LAYER_COUNT] = {
    "BACKGROUND", "BORDER", "ARTWORK", "OVERLAY", "HIGHLIGHT",
};
/* across: 0 the left edge, 1 the middle, 2 the right; up and down: 0 the bottom, 1 the middle, 2 the top */
static const int ACROSS[FRAMES_POINT_COUNT] = { 0, 1, 2, 0, 1, 2, 0, 1, 2 };
static const int UPDOWN[FRAMES_POINT_COUNT] = { 2, 2, 2, 1, 1, 1, 0, 0, 0 };
/* }}} */

/* {{{ the handle */
typedef struct {
    int is_region;
    char *type, *name;
    int parent;
    int *kids, nkids, capkids;            /* child frames, in order */
    int *regs, nregs, capregs;            /* regions, in order */
    FramesAnchor anchors[FRAMES_POINT_COUNT];
    int nanchors;
    double w, h;                          /* set size, own units */
    double scale;                         /* frames; 1 for regions */
    FramesStrata strata;                  /* as set; PARENT follows */
    int level, level_own;                 /* as set, and whether it was */
    int shown;
    long long shown_at;                   /* when it last became visible */
    FramesLayer layer;
    int sublevel;
    int clamped;
    double inset[4];                      /* left, right, top, bottom */
    /* worked out */
    FramesRect abs;                       /* screen units */
    FramesUnplaced why;
    int dirty;
    int carry;                            /* needed computing, but out of tries: dirty again for the next layout */
    unsigned tries_in;                    /* the layout `tries` counts for */
    int tries;
    unsigned cache_gen;                   /* tree_gen these were worked out at */
    double eff;
    int level_eff;
    FramesStrata strata_eff;
    int visible;
} Obj;

typedef struct {                          /* a draw order key */
    int a, b;
    long long c;
    int id;
} Key;

struct Frames {
    FramesOptions opt;
    Obj *o;
    int n, cap;
    double screen_w;                      /* interface units */
    /* names: open addressing, keys are the objects' own names */
    int *slots;
    int nslots, nnames;
    /* the chains */
    unsigned tree_gen;
    long long shown_clock;
    /* layout */
    int *dirty, ndirty;                   /* cap */
    int *notify, nnotify;                 /* cap; kept by realloc, as `resized` may make objects while it is read */
    int graph_changed;
    int in_layout;
    unsigned layout_id, pass_id;
    int *rev_start, *rev, nrev;           /* who is tied to whom: cap + 1, cap * 9 */
    int rev_n;                            /* objects the lists were built for */
    /* working arrays, all cap long (rev 9 x), in one block */
    int *list, *indeg, *queue, *chain, *stack;
    unsigned *in_pass, *moved, *done;
    int *t_index, *t_low, *t_stack, *t_on, *t_it;
    Key *keys, *keys2;
    void *work;
};
/* }}} */

/* {{{ small helpers */
static int valid(const Frames *f, int id) { return id >= 0 && id < f->n; }
static int is_frame(const Frames *f, int id) { return valid(f, id) && !f->o[id].is_region; }
static int is_number(double v) { return isfinite(v); }

static char *copy_text(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static unsigned long hash(const char *s)
{
    unsigned long h = 5381;
    for (; *s; ++s) h = h * 33 + (unsigned char)*s;
    return h;
}

/* the slot holding `name`, or the empty one where it would go */
static int slot_of(const Frames *f, const char *name)
{
    int i = (int)(hash(name) & (unsigned long)(f->nslots - 1));
    while (f->slots[i] != FRAMES_NONE && strcmp(f->o[f->slots[i]].name, name)) i = (i + 1) & (f->nslots - 1);
    return i;
}

static WErr name_put(Frames *f, int id, WWhy *why)
{
    if ((f->nnames + 1) * 2 > f->nslots) {
        int ns = f->nslots ? f->nslots * 2 : 256;
        int *old = f->slots, olds = f->nslots;
        int *s = malloc((size_t)ns * sizeof *s);
        if (!s) return w_fail(why, W_ERR_FULL, "no memory for the interface's frame names");
        for (int i = 0; i < ns; ++i) s[i] = FRAMES_NONE;
        f->slots = s;
        f->nslots = ns;
        for (int i = 0; i < olds; ++i) if (old[i] != FRAMES_NONE) f->slots[slot_of(f, f->o[old[i]].name)] = old[i];
        free(old);
    }
    int i = slot_of(f, f->o[id].name);
    if (f->slots[i] == FRAMES_NONE) ++f->nnames;
    f->slots[i] = id;
    return W_OK;
}

static int push_id(int **arr, int *n, int *cap, int id)
{
    if (*n == *cap) {
        int c = *cap ? *cap * 2 : 4;
        int *p = realloc(*arr, (size_t)c * sizeof *p);
        if (!p) return 0;
        *arr = p;
        *cap = c;
    }
    (*arr)[(*n)++] = id;
    return 1;
}
static void remove_id(int *arr, int *n, int id)
{
    for (int i = 0; i < *n; ++i)
        if (arr[i] == id) { memmove(arr + i, arr + i + 1, (size_t)(*n - i - 1) * sizeof *arr); --*n; return; }
}
/* }}} */

/* {{{ the chains: effective scale, strata, level, visible, kept per tree_gen */
static void chain_values(Frames *f, int id)
{
    int n = 0, c = id;
    while (c != FRAMES_NONE && f->o[c].cache_gen != f->tree_gen) { f->chain[n++] = c; c = f->o[c].parent; }
    while (n--) {
        Obj *o = &f->o[f->chain[n]];
        const Obj *p = o->parent != FRAMES_NONE ? &f->o[o->parent] : NULL;
        o->eff = (p ? p->eff : 1) * o->scale;
        if (o->is_region) { o->strata_eff = p->strata_eff; o->level_eff = p->level_eff; }
        else {
            o->strata_eff = o->strata != FRAMES_STRATA_PARENT ? o->strata : p ? p->strata_eff : FRAMES_MEDIUM;
            o->level_eff = o->level_own ? o->level : p ? (p->level_eff < FRAMES_MAX_LEVEL ? p->level_eff + 1 : FRAMES_MAX_LEVEL) : 0;
        }
        o->visible = o->shown && (!p || p->visible);
        o->cache_gen = f->tree_gen;
    }
}
static const Obj *chained(Frames *f, int id) { chain_values(f, id); return &f->o[id]; }
static void changed_chains(Frames *f) { if (++f->tree_gen == 0) { for (int i = 0; i < f->n; ++i) f->o[i].cache_gen = 0; f->tree_gen = 1; } }
/* }}} */

/* {{{ dirty marks */
static void mark(Frames *f, int id)
{
    if (f->o[id].dirty) return;
    f->o[id].dirty = 1;
    f->dirty[f->ndirty++] = id;
}
/* the object and everything under it (a scale or parent change) */
static void mark_under(Frames *f, int id)
{
    int sp = 0;
    f->stack[sp++] = id;
    while (sp) {
        const Obj *o = &f->o[f->stack[--sp]];
        mark(f, (int)(o - f->o));
        for (int i = 0; i < o->nregs; ++i) mark(f, o->regs[i]);
        for (int i = o->nkids - 1; i >= 0; --i) f->stack[sp++] = o->kids[i];
    }
}
/* frames under `id` (itself first) that became visible get the newest
 * shown times, parents before children, children in order */
static void stamp_shown(Frames *f, int id)
{
    int sp = 0;
    f->stack[sp++] = id;
    while (sp) {
        int c = f->stack[--sp];
        if (!chained(f, c)->visible) continue;
        f->o[c].shown_at = ++f->shown_clock;
        for (int i = f->o[c].nkids - 1; i >= 0; --i) f->stack[sp++] = f->o[c].kids[i];
    }
}
/* }}} */

/* {{{ making */
Frames *frames_new(const FramesOptions *opt, WWhy *why)
{
    Frames *f = calloc(1, sizeof *f);
    if (!f) { w_fail(why, W_ERR_FULL, "no memory for the interface's frames"); return NULL; }
    if (opt) f->opt = *opt;
    f->screen_w = FRAMES_SCREEN_HEIGHT * 1024.0 / 768.0;
    f->tree_gen = 1;
    return f;
}

void frames_free(Frames *f)
{
    if (!f) return;
    for (int i = 0; i < f->n; ++i) { free(f->o[i].type); free(f->o[i].name); free(f->o[i].kids); free(f->o[i].regs); }
    free(f->o);
    free(f->slots);
    free(f->dirty);
    free(f->notify);
    free(f->rev_start);
    free(f->rev);
    free(f->work);
    free(f);
}

/* room for one more object: the array and every working array */
static WErr room(Frames *f, WWhy *why)
{
    if (f->n < f->cap) return W_OK;
    int cap = f->cap ? f->cap * 2 : 64;
    size_t c = (size_t)cap;
    Obj *o = realloc(f->o, c * sizeof *o);
    if (!o) return w_fail(why, W_ERR_FULL, "no memory for another interface object");
    f->o = o;
    int *d = realloc(f->dirty, c * sizeof *d);
    if (!d) return w_fail(why, W_ERR_FULL, "no memory for another interface object");
    f->dirty = d;
    int *nt = realloc(f->notify, c * sizeof *nt);
    if (!nt) return w_fail(why, W_ERR_FULL, "no memory for another interface object");
    f->notify = nt;
    int *rs = malloc((c + 1) * sizeof *rs), *rv = malloc(c * FRAMES_POINT_COUNT * sizeof *rv);
    size_t ints = 10 * c, uns = 3 * c;
    void *w = malloc(ints * sizeof(int) + uns * sizeof(unsigned) + 2 * c * sizeof(Key) + 16);
    if (!rs || !rv || !w) { free(rs); free(rv); free(w); return w_fail(why, W_ERR_FULL, "no memory for another interface object"); }
    free(f->rev_start); free(f->rev); free(f->work);
    f->rev_start = rs; f->rev = rv; f->rev_n = 0; f->graph_changed = 1;
    f->work = w;
    Key *k = w;
    f->keys = k; f->keys2 = k + c;
    int *ip = (int *)(k + 2 * c);
    f->list = ip; f->indeg = ip + c; f->queue = ip + 2 * c; f->chain = ip + 3 * c; f->stack = ip + 4 * c;
    f->t_index = ip + 5 * c; f->t_low = ip + 6 * c; f->t_stack = ip + 7 * c; f->t_on = ip + 8 * c; f->t_it = ip + 9 * c;
    unsigned *up = (unsigned *)(ip + 10 * c);
    f->in_pass = up; f->moved = up + c; f->done = up + 2 * c;
    memset(up, 0, uns * sizeof *up);
    f->cap = cap;
    return W_OK;
}

static int new_object(Frames *f, int is_region, const char *type, const char *name, int parent, FramesLayer layer, int sublevel, WWhy *why)
{
    if (room(f, why)) return FRAMES_NONE;
    char *t = copy_text(type ? type : (is_region ? "Texture" : "Frame")), *nm = copy_text(name);
    if (!t || (name && !nm)) { free(t); free(nm); w_fail(why, W_ERR_FULL, "no memory for another interface object"); return FRAMES_NONE; }
    if (parent != FRAMES_NONE) {
        Obj *p = &f->o[parent];
        if (!(is_region ? push_id(&p->regs, &p->nregs, &p->capregs, f->n) : push_id(&p->kids, &p->nkids, &p->capkids, f->n))) {
            free(t); free(nm); w_fail(why, W_ERR_FULL, "no memory for another interface object"); return FRAMES_NONE;
        }
    }
    int id = f->n;
    Obj *o = &f->o[id];
    memset(o, 0, sizeof *o);
    o->is_region = is_region;
    o->type = t;
    o->name = nm;
    o->parent = parent;
    o->scale = 1;
    o->strata = FRAMES_STRATA_PARENT;
    o->shown = 1;
    o->layer = layer;
    o->sublevel = sublevel;
    o->why = FRAMES_NO_ANCHORS;
    if (nm && name_put(f, id, why)) {             /* no room for the name: undone */
        if (parent != FRAMES_NONE) { Obj *p = &f->o[parent]; if (is_region) --p->nregs; else --p->nkids; }
        free(t); free(nm);
        return FRAMES_NONE;
    }
    ++f->n;
    mark(f, id);
    f->graph_changed = 1;
    if (!is_region && chained(f, id)->visible) o->shown_at = ++f->shown_clock;
    return id;
}

int frames_new_frame(Frames *f, const char *type, const char *name, int parent, WWhy *why)
{
    if (parent != FRAMES_NONE && !is_frame(f, parent)) { w_fail(why, W_ERR_REFUSED, "the parent of a new frame must be a frame (%d isn't)", parent); return FRAMES_NONE; }
    return new_object(f, 0, type, name, parent, FRAMES_LAYER_ARTWORK, 0, why);
}

int frames_new_region(Frames *f, const char *type, const char *name, int parent, FramesLayer layer, int sublevel, WWhy *why)
{
    if (!is_frame(f, parent)) { w_fail(why, W_ERR_REFUSED, "a region must be made on a frame (%d isn't one)", parent); return FRAMES_NONE; }
    if ((int)layer < 0 || layer >= FRAMES_LAYER_COUNT) { w_fail(why, W_ERR_REFUSED, "%d isn't a draw layer", (int)layer); return FRAMES_NONE; }
    if (sublevel < FRAMES_SUBLEVEL_MIN || sublevel > FRAMES_SUBLEVEL_MAX) { w_fail(why, W_ERR_REFUSED, "sublevel %d is outside %d to %d", sublevel, FRAMES_SUBLEVEL_MIN, FRAMES_SUBLEVEL_MAX); return FRAMES_NONE; }
    return new_object(f, 1, type, name, parent, layer, sublevel, why);
}

int frames_find(const Frames *f, const char *name)
{
    if (!name || !f->nslots) return FRAMES_NONE;
    return f->slots[slot_of(f, name)];
}
int frames_count(const Frames *f) { return f->n; }
int frames_is_region(const Frames *f, int id) { return valid(f, id) && f->o[id].is_region; }
const char *frames_name(const Frames *f, int id) { return valid(f, id) ? f->o[id].name : NULL; }
const char *frames_type(const Frames *f, int id) { return valid(f, id) ? f->o[id].type : NULL; }
/* }}} */

/* {{{ the screen */
WErr frames_set_screen(Frames *f, double width_px, double height_px, WWhy *why)
{
    if (!(width_px > 0) || !(height_px > 0) || !is_number(width_px) || !is_number(height_px))
        return w_fail(why, W_ERR_REFUSED, "a screen of %g x %g pixels has no size", width_px, height_px);
    double w = FRAMES_SCREEN_HEIGHT * width_px / height_px;
    if (w == f->screen_w) return W_OK;
    f->screen_w = w;
    for (int i = 0; i < f->n; ++i) {
        const Obj *o = &f->o[i];
        int tied = o->clamped;
        for (int a = 0; a < o->nanchors && !tied; ++a) tied = o->anchors[a].relative == FRAMES_SCREEN;
        if (tied) mark(f, i);
    }
    return W_OK;
}
double frames_screen_width(const Frames *f) { return f->screen_w; }
/* }}} */

/* {{{ parents and children */
WErr frames_set_parent(Frames *f, int id, int parent, WWhy *why)
{
    if (!valid(f, id)) return w_fail(why, W_ERR_NOT_FOUND, "no interface object %d", id);
    Obj *o = &f->o[id];
    if (parent == FRAMES_NONE && o->is_region) return w_fail(why, W_ERR_REFUSED, "a region can't be left without a frame");
    if (parent != FRAMES_NONE && !is_frame(f, parent)) return w_fail(why, W_ERR_REFUSED, "%d can't be a parent: it isn't a frame", parent);
    for (int c = parent; c != FRAMES_NONE; c = f->o[c].parent)
        if (c == id) return w_fail(why, W_ERR_REFUSED, "%s can't be parented to %s, which is %s", o->name ? o->name : "an unnamed frame",
                                    f->o[parent].name ? f->o[parent].name : "an unnamed frame", parent == id ? "itself" : "under it");
    if (parent == o->parent) return W_OK;
    int was_visible = chained(f, id)->visible;
    if (parent != FRAMES_NONE) {                  /* room first, so a failure changes nothing */
        Obj *p = &f->o[parent];
        if (!(o->is_region ? push_id(&p->regs, &p->nregs, &p->capregs, id) : push_id(&p->kids, &p->nkids, &p->capkids, id)))
            return w_fail(why, W_ERR_FULL, "no memory to move an interface object");
    }
    if (o->parent != FRAMES_NONE) {
        Obj *q = &f->o[o->parent];
        if (o->is_region) remove_id(q->regs, &q->nregs, id); else remove_id(q->kids, &q->nkids, id);
    }
    o->parent = parent;
    changed_chains(f);
    mark_under(f, id);
    if (!o->is_region && !was_visible && chained(f, id)->visible) stamp_shown(f, id);
    return W_OK;
}
int frames_parent(const Frames *f, int id) { return valid(f, id) ? f->o[id].parent : FRAMES_NONE; }
int frames_child_count(const Frames *f, int id) { return valid(f, id) ? f->o[id].nkids : 0; }
int frames_child_at(const Frames *f, int id, int i) { return valid(f, id) && i >= 0 && i < f->o[id].nkids ? f->o[id].kids[i] : FRAMES_NONE; }
int frames_region_count(const Frames *f, int id) { return valid(f, id) ? f->o[id].nregs : 0; }
int frames_region_at(const Frames *f, int id, int i) { return valid(f, id) && i >= 0 && i < f->o[id].nregs ? f->o[id].regs[i] : FRAMES_NONE; }
/* }}} */

/* {{{ anchors, size, clamping */
WErr frames_set_point(Frames *f, int id, FramesPoint point, int relative, FramesPoint relative_point, double x, double y, WWhy *why)
{
    if (!valid(f, id)) return w_fail(why, W_ERR_NOT_FOUND, "no interface object %d", id);
    if ((int)point < 0 || point >= FRAMES_POINT_COUNT || (int)relative_point < 0 || relative_point >= FRAMES_POINT_COUNT)
        return w_fail(why, W_ERR_REFUSED, "an anchor's points must be two of the nine (%d, %d)", (int)point, (int)relative_point);
    if (!is_number(x) || !is_number(y)) return w_fail(why, W_ERR_REFUSED, "an anchor's offset must be a number");
    Obj *o = &f->o[id];
    if (relative == FRAMES_NONE) relative = o->parent != FRAMES_NONE ? o->parent : FRAMES_SCREEN;
    if (relative != FRAMES_SCREEN && !valid(f, relative)) return w_fail(why, W_ERR_NOT_FOUND, "no interface object %d to anchor to", relative);
    if (relative == id) return w_fail(why, W_ERR_REFUSED, "%s can't be anchored to itself", o->name ? o->name : "an unnamed object");
    FramesAnchor a = { point, relative, relative_point, x, y };
    int i = 0;
    while (i < o->nanchors && o->anchors[i].point != point) ++i;
    o->anchors[i] = a;
    if (i == o->nanchors) ++o->nanchors;
    mark(f, id);
    f->graph_changed = 1;
    return W_OK;
}

void frames_clear_points(Frames *f, int id)
{
    if (!valid(f, id) || !f->o[id].nanchors) return;
    f->o[id].nanchors = 0;
    mark(f, id);
    f->graph_changed = 1;
}

WErr frames_set_all_points(Frames *f, int id, int relative, WWhy *why)
{
    if (!valid(f, id)) return w_fail(why, W_ERR_NOT_FOUND, "no interface object %d", id);
    if (relative == FRAMES_NONE) relative = f->o[id].parent != FRAMES_NONE ? f->o[id].parent : FRAMES_SCREEN;
    if (relative != FRAMES_SCREEN && !valid(f, relative)) return w_fail(why, W_ERR_NOT_FOUND, "no interface object %d to anchor to", relative);
    if (relative == id) return w_fail(why, W_ERR_REFUSED, "%s can't be anchored to itself", f->o[id].name ? f->o[id].name : "an unnamed object");
    frames_clear_points(f, id);
    frames_set_point(f, id, FRAMES_TOPLEFT, relative, FRAMES_TOPLEFT, 0, 0, why);
    return frames_set_point(f, id, FRAMES_BOTTOMRIGHT, relative, FRAMES_BOTTOMRIGHT, 0, 0, why);
}

int frames_point_count(const Frames *f, int id) { return valid(f, id) ? f->o[id].nanchors : 0; }
int frames_point_at(const Frames *f, int id, int i, FramesAnchor *out)
{
    if (!valid(f, id) || i < 0 || i >= f->o[id].nanchors) return 0;
    *out = f->o[id].anchors[i];
    return 1;
}

static WErr set_size(Frames *f, int id, double v, double *field, WWhy *why)
{
    if (!valid(f, id)) return w_fail(why, W_ERR_NOT_FOUND, "no interface object %d", id);
    if (!is_number(v)) return w_fail(why, W_ERR_REFUSED, "a size must be a number");
    if (v < 0) v = 0;
    if (*field != v) { *field = v; mark(f, id); }
    return W_OK;
}
WErr frames_set_width(Frames *f, int id, double w, WWhy *why) { return set_size(f, id, w, valid(f, id) ? &f->o[id].w : NULL, why); }
WErr frames_set_height(Frames *f, int id, double h, WWhy *why) { return set_size(f, id, h, valid(f, id) ? &f->o[id].h : NULL, why); }
double frames_set_width_of(const Frames *f, int id) { return valid(f, id) ? f->o[id].w : 0; }
double frames_set_height_of(const Frames *f, int id) { return valid(f, id) ? f->o[id].h : 0; }

void frames_set_clamped(Frames *f, int id, int on)
{
    if (!valid(f, id) || f->o[id].clamped == !!on) return;
    f->o[id].clamped = !!on;
    mark(f, id);
}
void frames_set_clamp_insets(Frames *f, int id, double left, double right, double top, double bottom)
{
    if (!valid(f, id) || !is_number(left) || !is_number(right) || !is_number(top) || !is_number(bottom)) return;
    double *in = f->o[id].inset;
    in[0] = left; in[1] = right; in[2] = top; in[3] = bottom;
    mark(f, id);
}
/* }}} */

/* {{{ layout */
/* who is tied to whom, as lists by the object tied to */
static void build_reverse(Frames *f)
{
    int n = f->n;
    memset(f->rev_start, 0, (size_t)(n + 1) * sizeof *f->rev_start);
    for (int i = 0; i < n; ++i)
        for (int a = 0; a < f->o[i].nanchors; ++a) { int r = f->o[i].anchors[a].relative; if (r >= 0) ++f->rev_start[r + 1]; }
    for (int i = 0; i < n; ++i) f->rev_start[i + 1] += f->rev_start[i];
    int *fill = f->indeg;                             /* free outside a pass */
    memcpy(fill, f->rev_start, (size_t)n * sizeof *fill);
    for (int i = 0; i < n; ++i)
        for (int a = 0; a < f->o[i].nanchors; ++a) { int r = f->o[i].anchors[a].relative; if (r >= 0) f->rev[fill[r]++] = i; }
    f->nrev = f->rev_start[n];
    f->rev_n = n;
    f->graph_changed = 0;
}

static void point_at(const FramesRect *r, FramesPoint p, double *x, double *y)
{
    int a = ACROSS[p], u = UPDOWN[p];
    *x = a == 0 ? r->left : a == 2 ? r->right : (r->left + r->right) * 0.5;
    *y = u == 0 ? r->bottom : u == 2 ? r->top : (r->bottom + r->top) * 0.5;
}

/* one axis from its edges given (have[0] low, [1] middle, [2] high) and its size */
static void axis(const int have[3], const double at[3], double size, double *lo, double *hi)
{
    if (have[0] && have[2]) { *lo = at[0]; *hi = at[2]; }
    else if (have[0] && have[1]) { *lo = at[0]; *hi = 2 * at[1] - at[0]; }
    else if (have[1] && have[2]) { *hi = at[2]; *lo = 2 * at[1] - at[2]; }
    else if (have[0]) { *lo = at[0]; *hi = at[0] + size; }
    else if (have[2]) { *hi = at[2]; *lo = at[2] - size; }
    else { *lo = at[1] - size * 0.5; *hi = at[1] + size * 0.5; }
}

static void compute(Frames *f, int id, FramesRect *r, FramesUnplaced *why)
{
    Obj *o = &f->o[id];
    memset(r, 0, sizeof *r);
    if (!o->nanchors) { *why = FRAMES_NO_ANCHORS; return; }
    double eff = chained(f, id)->eff;
    const FramesRect screen = { 1, 0, f->screen_w, 0, FRAMES_SCREEN_HEIGHT };
    int hx[3] = { 0 }, hy[3] = { 0 };
    double ax[3] = { 0 }, ay[3] = { 0 };
    for (int i = 0; i < o->nanchors; ++i) {
        const FramesAnchor *a = &o->anchors[i];
        const FramesRect *rel = a->relative == FRAMES_SCREEN ? &screen : &f->o[a->relative].abs;
        if (!rel->placed) { *why = FRAMES_WAITS; return; }
        double x, y;
        point_at(rel, a->relative_point, &x, &y);
        x += a->x * eff;
        y += a->y * eff;
        int kx = ACROSS[a->point], ky = UPDOWN[a->point];
        if (!hx[kx]) { hx[kx] = 1; ax[kx] = x; }      /* the first anchor giving an edge gives it */
        if (!hy[ky]) { hy[ky] = 1; ay[ky] = y; }
    }
    axis(hx, ax, o->w * eff, &r->left, &r->right);
    axis(hy, ay, o->h * eff, &r->bottom, &r->top);
    if (o->clamped) {
        double l = r->left + o->inset[0] * eff, rt = r->right + o->inset[1] * eff;
        double t = r->top + o->inset[2] * eff, b = r->bottom + o->inset[3] * eff;
        double dx = 0, dy = 0;
        if (l < 0) dx = -l;
        else if (rt > f->screen_w) { dx = f->screen_w - rt; if (l + dx < 0) dx = -l; }       /* wider than the screen: the left edge stays */
        if (t > FRAMES_SCREEN_HEIGHT) dy = FRAMES_SCREEN_HEIGHT - t;
        else if (b < 0) { dy = -b; if (t + dy > FRAMES_SCREEN_HEIGHT) dy = FRAMES_SCREEN_HEIGHT - t; }  /* taller: the top stays */
        r->left += dx; r->right += dx; r->bottom += dy; r->top += dy;
    }
    r->placed = 1;
    *why = FRAMES_PLACED;
}

static int moved_by(const FramesRect *a, const FramesRect *b)
{
    if (a->placed != b->placed) return 1;
    return fabs(a->left - b->left) >= FRAMES_MOVE_EPSILON || fabs(a->right - b->right) >= FRAMES_MOVE_EPSILON
        || fabs(a->bottom - b->bottom) >= FRAMES_MOVE_EPSILON || fabs(a->top - b->top) >= FRAMES_MOVE_EPSILON;
}
static int resized_by(const FramesRect *a, const FramesRect *b)
{
    if (a->placed != b->placed) return b->placed;
    return fabs((a->right - a->left) - (b->right - b->left)) >= FRAMES_MOVE_EPSILON
        || fabs((a->top - a->bottom) - (b->top - b->bottom)) >= FRAMES_MOVE_EPSILON;
}

/* The objects left over in a pass (in_pass, not done) are waiting on a
 * circle: those in one (a strongly connected group of two or more,
 * through anchors among them) are FRAMES_IN_LOOP, the rest FRAMES_WAITS.
 * Tarjan's way, without recursion. */
static void find_loops(Frames *f, const int *left, int nleft)
{
    unsigned p = f->pass_id;
    int index = 0, sp = 0;
    for (int k = 0; k < nleft; ++k) f->t_index[left[k]] = -1, f->t_on[left[k]] = 0;
    for (int k = 0; k < nleft; ++k) {
        int root = left[k];
        if (f->t_index[root] >= 0) continue;
        int cs = 0;                                       /* the walk's own stack, in `chain` */
        f->chain[cs++] = root;
        f->t_index[root] = f->t_low[root] = index++;
        f->t_it[root] = 0;
        f->t_stack[sp++] = root; f->t_on[root] = 1;
        while (cs) {
            int v = f->chain[cs - 1];
            const Obj *o = &f->o[v];
            if (f->t_it[v] < o->nanchors) {
                int w = o->anchors[f->t_it[v]++].relative;
                if (w < 0 || f->in_pass[w] != p || f->done[w] == p) continue;
                if (f->t_index[w] < 0) {
                    f->t_index[w] = f->t_low[w] = index++;
                    f->t_it[w] = 0;
                    f->t_stack[sp++] = w; f->t_on[w] = 1;
                    f->chain[cs++] = w;
                } else if (f->t_on[w] && f->t_index[w] < f->t_low[v]) f->t_low[v] = f->t_index[w];
                continue;
            }
            if (f->t_low[v] == f->t_index[v]) {           /* v heads a group: pop it */
                int start = sp;
                do --start; while (f->t_stack[start] != v);
                int size = sp - start;
                for (int i = start; i < sp; ++i) { f->t_on[f->t_stack[i]] = 0; f->o[f->t_stack[i]].why = size > 1 ? FRAMES_IN_LOOP : FRAMES_WAITS; }
                sp = start;
            }
            --cs;
            if (cs) { int u = f->chain[cs - 1]; if (f->t_low[v] < f->t_low[u]) f->t_low[u] = f->t_low[v]; }
        }
    }
}

/* one pass; how many objects it computed */
static int pass(Frames *f)
{
    unsigned p = ++f->pass_id;
    if (!p) {                                             /* wrapped: start the marks again */
        memset(f->in_pass, 0, (size_t)f->cap * 3 * sizeof *f->in_pass);
        p = f->pass_id = 1;
    }
    /* the dirty objects and all tied to them */
    int n = 0;
    for (int i = 0; i < f->ndirty; ++i) { int d = f->dirty[i]; if (f->in_pass[d] != p) { f->in_pass[d] = p; f->list[n++] = d; } }
    for (int i = 0; i < n; ++i) {
        int s = f->list[i];
        if (s >= f->rev_n) continue;
        for (int k = f->rev_start[s]; k < f->rev_start[s + 1]; ++k) {
            int d = f->rev[k];
            if (f->in_pass[d] != p) { f->in_pass[d] = p; f->list[n++] = d; }
        }
    }
    /* dependency order: an object once everything it is tied to in this pass is done */
    int q = 0, qn = 0;
    for (int i = 0; i < n; ++i) {
        int s = f->list[i], deg = 0;
        for (int a = 0; a < f->o[s].nanchors; ++a) { int r = f->o[s].anchors[a].relative; if (r >= 0 && f->in_pass[r] == p) ++deg; }
        f->indeg[s] = deg;
        if (!deg) f->queue[qn++] = s;
    }
    f->nnotify = 0;
    int computed = 0;
    while (q < qn) {
        int s = f->queue[q++];
        Obj *o = &f->o[s];
        f->done[s] = p;
        int need = o->dirty;
        for (int a = 0; a < o->nanchors && !need; ++a) { int r = o->anchors[a].relative; need = r >= 0 && f->moved[r] == p; }
        if (o->tries_in != f->layout_id) { o->tries_in = f->layout_id; o->tries = 0; }
        if (need && o->tries >= FRAMES_MAX_TRIES) o->carry = 1;
        else if (need) {
            ++o->tries;
            ++computed;
            FramesRect r;
            FramesUnplaced why;
            compute(f, s, &r, &why);
            int changed_why = why != o->why;          /* unplaced for another reason: what waits on it may be too */
            o->why = why;
            if (changed_why || moved_by(&o->abs, &r)) {
                if (resized_by(&o->abs, &r)) f->notify[f->nnotify++] = s;
                o->abs = r;
                f->moved[s] = p;
            }
        }
        if (s < f->rev_n)
            for (int k = f->rev_start[s]; k < f->rev_start[s + 1]; ++k) {
                int d = f->rev[k];
                if (f->in_pass[d] == p && --f->indeg[d] == 0) f->queue[qn++] = d;
            }
    }
    /* what never came up is waiting on a circle */
    if (qn < n) {
        int nleft = 0;
        for (int i = 0; i < n; ++i) if (f->done[f->list[i]] != p) f->stack[nleft++] = f->list[i];
        find_loops(f, f->stack, nleft);
        for (int i = 0; i < nleft; ++i) memset(&f->o[f->stack[i]].abs, 0, sizeof(FramesRect));
    }
    for (int i = 0; i < f->ndirty; ++i) f->o[f->dirty[i]].dirty = 0;
    f->ndirty = 0;
    for (int i = 0; i < n; ++i) if (f->o[f->list[i]].carry) { f->o[f->list[i]].carry = 0; mark(f, f->list[i]); }
    return computed;
}

void frames_layout(Frames *f)
{
    if (f->in_layout || !f->ndirty) return;
    f->in_layout = 1;
    if (++f->layout_id == 0) { for (int i = 0; i < f->n; ++i) f->o[i].tries_in = 0; f->layout_id = 1; }
    for (int k = 0; k < FRAMES_MAX_PASSES && f->ndirty; ++k) {
        if (f->graph_changed || f->rev_n != f->n) build_reverse(f);
        if (!pass(f)) break;                              /* nothing could be computed: what is left waits for the next layout */
        if (f->opt.resized)
            for (int i = 0; i < f->nnotify; ++i) {        /* re-read each time: `resized` may make objects (notify is kept by realloc) */
                int id = f->notify[i];
                f->opt.resized(f->opt.ctx, id);
            }
        f->nnotify = 0;
    }
    f->in_layout = 0;
}
/* }}} */

/* {{{ reading back */
void frames_rect_screen(Frames *f, int id, FramesRect *out)
{
    memset(out, 0, sizeof *out);
    if (!valid(f, id)) return;
    frames_layout(f);
    *out = f->o[id].abs;
}
void frames_rect(Frames *f, int id, FramesRect *out)
{
    frames_rect_screen(f, id, out);
    if (!out->placed) return;
    double e = chained(f, id)->eff;
    out->left /= e; out->right /= e; out->bottom /= e; out->top /= e;
}
FramesUnplaced frames_unplaced(Frames *f, int id)
{
    if (!valid(f, id)) return FRAMES_NO_ANCHORS;
    frames_layout(f);
    return f->o[id].abs.placed ? FRAMES_PLACED : f->o[id].why;
}
static double size_read(Frames *f, int id, int across)
{
    if (!valid(f, id)) return 0;
    frames_layout(f);
    const Obj *o = &f->o[id];
    double set = across ? o->w : o->h;
    if (!f->opt.size_from_layout && set > 0) return set;
    if (!o->abs.placed) return set;
    double e = chained(f, id)->eff;
    return (across ? o->abs.right - o->abs.left : o->abs.top - o->abs.bottom) / e;
}
double frames_width(Frames *f, int id) { return size_read(f, id, 1); }
double frames_height(Frames *f, int id) { return size_read(f, id, 0); }
/* }}} */

/* {{{ scale */
WErr frames_set_scale(Frames *f, int id, double scale, WWhy *why)
{
    if (!valid(f, id)) return w_fail(why, W_ERR_NOT_FOUND, "no interface object %d", id);
    if (f->o[id].is_region) return w_fail(why, W_ERR_REFUSED, "a region has no scale of its own");
    if (!is_number(scale) || !(scale > 0)) return w_fail(why, W_ERR_REFUSED, "a scale must be above 0 (not %g)", scale);
    if (fabs(scale - f->o[id].scale) < FRAMES_SCALE_EPSILON) return W_OK;
    f->o[id].scale = scale;
    changed_chains(f);
    mark_under(f, id);
    return W_OK;
}
double frames_scale(const Frames *f, int id) { return valid(f, id) ? f->o[id].scale : 1; }
double frames_effective_scale(Frames *f, int id) { return valid(f, id) ? chained(f, id)->eff : 1; }
/* }}} */

/* {{{ strata and levels */
WErr frames_set_strata(Frames *f, int id, FramesStrata strata, WWhy *why)
{
    if (!valid(f, id)) return w_fail(why, W_ERR_NOT_FOUND, "no interface object %d", id);
    if (f->o[id].is_region) return w_fail(why, W_ERR_REFUSED, "a region has no strata of its own");
    if ((int)strata < 0 || strata >= FRAMES_STRATA_COUNT) return w_fail(why, W_ERR_REFUSED, "%d isn't a frame strata", (int)strata);
    f->o[id].strata = strata;
    changed_chains(f);
    return W_OK;
}
FramesStrata frames_strata(Frames *f, int id) { return valid(f, id) ? chained(f, id)->strata_eff : FRAMES_MEDIUM; }

static WErr set_level(Frames *f, int id, int level, int limited, WWhy *why)
{
    if (!valid(f, id)) return w_fail(why, W_ERR_NOT_FOUND, "no interface object %d", id);
    if (f->o[id].is_region) return w_fail(why, W_ERR_REFUSED, "a region has no frame level of its own");
    if (level < 0) level = 0;
    if (level > FRAMES_MAX_LEVEL) level = FRAMES_MAX_LEVEL;
    int now = chained(f, id)->level_eff;
    if (limited && !f->opt.raise_unlimited && level > now + FRAMES_MAX_RAISE) level = now + FRAMES_MAX_RAISE;   /* the quirk */
    f->o[id].level = level;
    f->o[id].level_own = 1;
    changed_chains(f);
    return W_OK;
}
WErr frames_set_level(Frames *f, int id, int level, WWhy *why) { return set_level(f, id, level, 1, why); }
WErr frames_set_level_direct(Frames *f, int id, int level, WWhy *why) { return set_level(f, id, level, 0, why); }
int frames_level(Frames *f, int id) { return valid(f, id) ? chained(f, id)->level_eff : 0; }
/* }}} */

/* {{{ shown, hidden, visible */
void frames_show(Frames *f, int id)
{
    if (!valid(f, id) || f->o[id].shown) return;
    int was = chained(f, id)->visible;
    f->o[id].shown = 1;
    changed_chains(f);
    if (!was && !f->o[id].is_region) stamp_shown(f, id);
}
void frames_hide(Frames *f, int id)
{
    if (!valid(f, id) || !f->o[id].shown) return;
    f->o[id].shown = 0;
    changed_chains(f);
}
int frames_is_shown(const Frames *f, int id) { return valid(f, id) && f->o[id].shown; }
int frames_is_visible(Frames *f, int id) { return valid(f, id) && chained(f, id)->visible; }
/* }}} */

/* {{{ draw layers */
WErr frames_set_draw_layer(Frames *f, int id, FramesLayer layer, int sublevel, WWhy *why)
{
    if (!valid(f, id)) return w_fail(why, W_ERR_NOT_FOUND, "no interface object %d", id);
    if (!f->o[id].is_region) return w_fail(why, W_ERR_REFUSED, "a frame has no draw layer; its regions do");
    if ((int)layer < 0 || layer >= FRAMES_LAYER_COUNT) return w_fail(why, W_ERR_REFUSED, "%d isn't a draw layer", (int)layer);
    if (sublevel < FRAMES_SUBLEVEL_MIN || sublevel > FRAMES_SUBLEVEL_MAX)
        return w_fail(why, W_ERR_REFUSED, "sublevel %d is outside %d to %d", sublevel, FRAMES_SUBLEVEL_MIN, FRAMES_SUBLEVEL_MAX);
    f->o[id].layer = layer;
    f->o[id].sublevel = sublevel;
    return W_OK;
}
void frames_draw_layer(const Frames *f, int id, FramesLayer *layer, int *sublevel)
{
    *layer = valid(f, id) ? f->o[id].layer : FRAMES_LAYER_ARTWORK;
    *sublevel = valid(f, id) ? f->o[id].sublevel : 0;
}
/* }}} */

/* {{{ the order of drawing */
static int key_cmp(const void *pa, const void *pb)
{
    const Key *a = pa, *b = pb;
    if (a->a != b->a) return a->a < b->a ? -1 : 1;
    if (a->b != b->b) return a->b < b->b ? -1 : 1;
    if (a->c != b->c) return a->c < b->c ? -1 : 1;
    return (a->id > b->id) - (a->id < b->id);
}

int frames_draw_order(Frames *f, FramesDrawItem *out, int cap)
{
    frames_layout(f);
    int nf = 0, total = 0;
    for (int i = 0; i < f->n; ++i) {
        if (f->o[i].is_region) continue;
        const Obj *o = chained(f, i);
        if (o->visible) f->keys[nf++] = (Key){ (int)o->strata_eff, o->level_eff, o->shown_at, i };
    }
    if (nf > 1) qsort(f->keys, (size_t)nf, sizeof *f->keys, key_cmp);
    for (int k = 0; k < nf; ++k) {
        const Obj *o = &f->o[f->keys[k].id];
        int fid = f->keys[k].id;
        if (o->abs.placed) {
            if (total < cap) out[total] = (FramesDrawItem){ fid, fid, 0, o->strata_eff, o->level_eff, FRAMES_LAYER_BACKGROUND, 0 };
            ++total;
        }
        int nr = 0;
        for (int i = 0; i < o->nregs; ++i) {
            const Obj *r = &f->o[o->regs[i]];
            if (r->shown && r->abs.placed) f->keys2[nr++] = (Key){ (int)r->layer, r->sublevel, 0, o->regs[i] };
        }
        if (nr > 1) qsort(f->keys2, (size_t)nr, sizeof *f->keys2, key_cmp);
        for (int i = 0; i < nr; ++i) {
            if (total < cap) out[total] = (FramesDrawItem){ f->keys2[i].id, fid, 1, o->strata_eff, o->level_eff, (FramesLayer)f->keys2[i].a, f->keys2[i].b };
            ++total;
        }
    }
    return total;
}
/* }}} */

/* {{{ names */
static int index_of(const char *const *names, int n, const char *name)
{
    if (!name) return n;
    for (int i = 0; i < n; ++i) if (!strcasecmp(names[i], name)) return i;
    return n;
}
FramesPoint  frames_point_from_name(const char *name) { return (FramesPoint)index_of(POINT_NAMES, FRAMES_POINT_COUNT, name); }
FramesStrata frames_strata_from_name(const char *name) { return (FramesStrata)index_of(STRATA_NAMES, FRAMES_STRATA_COUNT, name); }
FramesLayer  frames_layer_from_name(const char *name) { return (FramesLayer)index_of(LAYER_NAMES, FRAMES_LAYER_COUNT, name); }
const char *frames_point_name(FramesPoint p) { return (int)p >= 0 && p < FRAMES_POINT_COUNT ? POINT_NAMES[p] : NULL; }
const char *frames_strata_name(FramesStrata s) { return (int)s >= 0 && s < FRAMES_STRATA_COUNT ? STRATA_NAMES[s] : NULL; }
const char *frames_layer_name(FramesLayer l) { return (int)l >= 0 && l < FRAMES_LAYER_COUNT ? LAYER_NAMES[l] : NULL; }
/* }}} */
