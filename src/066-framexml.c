/*
 * 066-framexml.c (issue 702) - see 066-framexml.h.
 *
 * Everything the handle keeps (elements, objects, warnings, its two name
 * tables) lives in one arena of blocks, freed together by fxml_free, so
 * nothing is freed piece by piece and running out of memory anywhere
 * leaves by one jump back to the public call that was running, which
 * says so (W_ERR_FULL); what was whole by then stays usable.
 *
 * A file is first read whole into elements (065-xml gives the pieces),
 * so a file that isn't well-formed is refused before anything in it is
 * done. Then its top-level elements are done in order.
 */
#include "066-framexml.h"
#include "065-xml.h"
#include "013-errors.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* {{{ the widget types made from XML (the original program's own list:
 * frame types, the two regions, fonts; regions also as their named slots) */
static const char *const FRAME_TYPES[] = {
    "Frame", "Button", "CheckButton", "ColorSelect", "Cooldown", "EditBox", "GameTooltip", "MessageFrame",
    "Minimap", "Model", "PlayerModel", "DressUpModel", "TabardModel", "MovieFrame", "QuestPOIFrame",
    "ScrollFrame", "ScrollingMessageFrame", "SimpleHTML", "Slider", "StatusBar", "TaxiRouteFrame",
    "WorldFrame", "UnitButton",
};
static const char *const REGION_TYPES[] = {
    "Texture", "FontString",
    /* slots: a region written as a named part of its frame */
    "NormalTexture", "PushedTexture", "DisabledTexture", "HighlightTexture", "CheckedTexture",
    "DisabledCheckedTexture", "ThumbTexture", "BarTexture", "ColorWheelTexture", "ColorWheelThumbTexture",
    "ColorValueTexture", "ColorValueThumbTexture", "ButtonText",
};
static const char *const POINTS[] = {
    "TOPLEFT", "TOP", "TOPRIGHT", "LEFT", "CENTER", "RIGHT", "BOTTOMLEFT", "BOTTOM", "BOTTOMRIGHT",
};
#define COUNT(a) ((int)(sizeof (a) / sizeof (a)[0]))
/* }}} */

/* {{{ the interface log's wording, one line a kind, each given up to two
 * strings in order. Where it is marked "log", the words are the
 * original's as public FrameXML.log copies show them (unchecked against
 * note 1.10, which this copy of docs/057 doesn't hold yet); the rest are
 * this client's own until the note gives the original's. */
static const char *const WORDING[FXML_W_COUNT] = {
    [FXML_W_NOT_XML]         = "%s isn't well-formed XML, so none of it is loaded: %s",
    [FXML_W_NOT_OPENED]      = "Couldn't open %s: %s",
    [FXML_W_NOT_UI]          = "%s: its top element is <%s>, not <Ui>, so none of it is loaded",
    [FXML_W_INCLUDE_NO_FILE] = "<Include> with no file, skipped%s%s",
    [FXML_W_INCLUDE_DEEP]    = "Include of %s is nested more than %s files deep, skipped",
    [FXML_W_TOP_UNKNOWN]     = "Unrecognized XML: %s%s",                                   /* log */
    [FXML_W_VIRTUAL_UNNAMED] = "A virtual %s has no name, so it can't be inherited; skipped%s",
    [FXML_W_VIRTUAL_NESTED]  = "%s is marked virtual inside another element; made as an ordinary %s",
    [FXML_W_TEMPLATE_TAKEN]  = "Deferred XML Node object named %s already exists%s",          /* log */
    [FXML_W_NO_TEMPLATE]     = "Couldn't find inherited node \"%s\"%s",                       /* log */
    [FXML_W_SELF_INHERIT]    = "%s inherits itself (named in the inherits of %s); skipped there",
    [FXML_W_UNKNOWN_PARENT]  = "%s: couldn't find its parent %s, so it has none",
    [FXML_W_ANCHOR_POINT]    = "%s: %s isn't an anchor point, anchor skipped",
    [FXML_W_ANCHOR_UNKNOWN]  = "%s: couldn't find the frame %s to anchor to, anchor skipped",
    [FXML_W_ANCHOR_SELF]     = "%s is anchored to itself, anchor skipped%s",
    [FXML_W_ALL_POINTS]      = "%s has setAllPoints and anchors; setAllPoints ignored%s",
    [FXML_W_TOO_BIG]         = "%s goes past %s; the rest of it isn't made",
};
/* }}} */

/* {{{ the arena, growing lists and name tables */
typedef struct Block {
    struct Block *next;
    size_t used, cap;
    _Alignas(16) unsigned char data[];
} Block;

typedef struct {
    const char **keys;
    void **vals;
    size_t cap, n;
    int nocase;
} Map;

typedef struct { void *p; size_t n, cap; } Vec;

struct Fxml {
    FxmlOptions opt;
    Block *blocks;
    Map templates;                        /* name (any case) -> FxmlNode */
    Map names;                            /* name -> FxmlObject, the last made */
    Vec steps;                            /* FxmlStep */
    Vec warnings;                         /* FxmlWarning */
    jmp_buf out;                          /* where running out of memory goes */
    int entered;                          /* public calls under way (Include nests them) */
    XmlReader *reader;                    /* the reader of the file being read, freed on the way out */
    char *bytes[FXML_MAX_INCLUDES + 1];   /* the Includes' bytes being read, likewise */
};

static _Noreturn void no_memory(Fxml *fx) { longjmp(fx->out, 1); }

static void *alloc(Fxml *fx, size_t n)
{
    n = (n + 15) & ~(size_t)15;
    Block *b = fx->blocks;
    if (!b || b->cap - b->used < n) {
        size_t cap = n > 60000 ? n : 60000;
        b = malloc(sizeof(Block) + cap);
        if (!b) no_memory(fx);
        b->next = fx->blocks;
        b->used = 0;
        b->cap = cap;
        fx->blocks = b;
    }
    void *p = b->data + b->used;
    b->used += n;
    memset(p, 0, n);
    return p;
}
static char *str_n(Fxml *fx, const char *s, size_t n)
{
    char *p = alloc(fx, n + 1);
    memcpy(p, s, n);
    p[n] = 0;
    return p;
}
static char *str(Fxml *fx, const char *s) { return s ? str_n(fx, s, strlen(s)) : NULL; }

static void vec_push(Fxml *fx, Vec *v, const void *item, size_t size)
{
    if (v->n == v->cap) {
        size_t cap = v->cap ? v->cap * 2 : 8;
        void *p = alloc(fx, cap * size);
        if (v->n) memcpy(p, v->p, v->n * size);
        v->p = p;
        v->cap = cap;
    }
    memcpy((char *)v->p + v->n * size, item, size);
    ++v->n;
}
static void vec_ptr(Fxml *fx, Vec *v, const void *p) { vec_push(fx, v, &p, sizeof p); }
#define AT(v, T, i) (((T *)(v).p)[i])

static uint64_t hash(const char *s, int nocase)
{
    uint64_t h = 1469598103934665603ull;
    for (; *s; ++s) {
        unsigned char c = (unsigned char)*s;
        if (nocase && c >= 'A' && c <= 'Z') c = (unsigned char)(c + 32);
        h = (h ^ c) * 1099511628211ull;
    }
    return h;
}
static int same(const char *a, const char *b, int nocase) { return nocase ? !strcasecmp(a, b) : !strcmp(a, b); }
static size_t map_slot(const Map *m, const char *key)
{
    size_t i = (size_t)hash(key, m->nocase) & (m->cap - 1);
    while (m->keys[i] && !same(m->keys[i], key, m->nocase)) i = (i + 1) & (m->cap - 1);
    return i;
}
static void *map_get(const Map *m, const char *key)
{
    if (!m->cap || !key) return NULL;
    size_t i = map_slot(m, key);
    return m->keys[i] ? m->vals[i] : NULL;
}
static void map_put(Fxml *fx, Map *m, const char *key, void *val)
{
    if ((m->n + 1) * 2 > m->cap) {
        Map g = { .cap = m->cap ? m->cap * 2 : 64, .nocase = m->nocase };
        g.keys = alloc(fx, g.cap * sizeof *g.keys);
        g.vals = alloc(fx, g.cap * sizeof *g.vals);
        for (size_t k = 0; k < m->cap; ++k)
            if (m->keys[k]) { size_t i = map_slot(&g, m->keys[k]); g.keys[i] = m->keys[k]; g.vals[i] = m->vals[k]; ++g.n; }
        *m = g;
    }
    size_t i = map_slot(m, key);
    if (!m->keys[i]) { m->keys[i] = str(fx, key); ++m->n; }
    m->vals[i] = val;
}
/* }}} */

/* {{{ small helpers: element kinds, true values, the log */
static int is_trusted(const char *taint) { return !taint || !taint[0]; }
static int is_true(const char *v) { return v && !strcasecmp(v, "true"); }
static const char *canonical(const char *const *list, int n, const char *tag)
{
    for (int i = 0; i < n; ++i) if (!strcasecmp(list[i], tag)) return list[i];
    return NULL;
}
/* the kind of object a tag makes, 0 for none; *canon its name as listed */
static FxmlKind kind_of(const char *tag, const char **canon)
{
    const char *c;
    if ((c = canonical(FRAME_TYPES, COUNT(FRAME_TYPES), tag))) { if (canon) *canon = c; return FXML_FRAME; }
    if ((c = canonical(REGION_TYPES, COUNT(REGION_TYPES), tag))) { if (canon) *canon = c; return FXML_REGION; }
    if (!strcasecmp(tag, "Font")) { if (canon) *canon = "Font"; return FXML_FONT; }
    return 0;
}

const char *fxml_attr(const FxmlNode *n, const char *name)
{
    for (int i = 0; n && i < n->nattrs; ++i)
        if (!strcasecmp(n->attrs[i].name, name)) return n->attrs[i].value;
    return NULL;
}
const char *fxml_set(const FxmlObject *o, const char *name)
{
    for (int i = 0; o && i < o->nsets; ++i)
        if (!strcasecmp(o->sets[i].name, name)) return o->sets[i].value;
    return NULL;
}

static void warn(Fxml *fx, FxmlWarnKind kind, const char *file, int line, const char *a, const char *b)
{
    char text[700];
    snprintf(text, sizeof text, WORDING[kind], a ? a : "", b ? b : "");
    FxmlWarning w = { kind, str(fx, file), line, str(fx, text) };
    vec_push(fx, &fx->warnings, &w, sizeof w);
    if (fx->opt.warn) fx->opt.warn(fx->opt.ctx, &w);
}
static void warn_at(Fxml *fx, FxmlWarnKind kind, const FxmlNode *n, const char *a, const char *b)
{
    warn(fx, kind, n->file->path, n->line, a, b);
}
/* how a log line names an object: its name, else its type */
static const char *describe(Fxml *fx, const char *type, const char *name)
{
    char d[300];
    if (name) snprintf(d, sizeof d, "%s %s", type, name);
    else snprintf(d, sizeof d, "an unnamed %s", type);
    return str(fx, d);
}
/* }}} */

/* {{{ static read_file: the bytes into elements; NULL, with why, when not well-formed */
typedef struct {
    FxmlNode *node;
    Vec kids;
    Vec text;
} Open;

static FxmlNode *read_file(Fxml *fx, FxmlFile *file, const char *bytes, size_t len, WWhy *why)
{
    fx->reader = xml_reader_new(bytes, len, why);
    if (!fx->reader) no_memory(fx);
    Vec stack = { 0 };
    FxmlNode *root = NULL;
    XmlEvent ev;
    for (;;) {
        WErr e = xml_next(fx->reader, &ev, why);
        if (e == W_ERR_FULL) no_memory(fx);
        if (e) { root = NULL; break; }
        if (ev.kind == XML_DONE) break;
        if (ev.kind == XML_START) {
            FxmlNode *n = alloc(fx, sizeof *n);
            n->tag = str(fx, ev.name);
            n->line = ev.line;
            n->file = file;
            n->text = "";
            if (ev.nattrs) {
                FxmlAttr *a = alloc(fx, (size_t)ev.nattrs * sizeof *a);
                for (int i = 0; i < ev.nattrs; ++i) a[i] = (FxmlAttr){ str(fx, ev.attrs[i].name), str(fx, ev.attrs[i].value) };
                n->attrs = a;
                n->nattrs = ev.nattrs;
            }
            if (stack.n) {
                Open *up = &AT(stack, Open, stack.n - 1);
                n->up = up->node;
                vec_ptr(fx, &up->kids, n);
            } else root = n;
            Open o = { .node = n };
            vec_push(fx, &stack, &o, sizeof o);
        } else if (ev.kind == XML_TEXT) {
            Open *o = &AT(stack, Open, stack.n - 1);
            if (!o->node->text_line) o->node->text_line = ev.line;
            for (size_t i = 0; i < ev.text_len; ++i) vec_push(fx, &o->text, ev.text + i, 1);
        } else {                              /* XML_END */
            Open *o = &AT(stack, Open, stack.n - 1);
            o->node->kids = o->kids.p;
            o->node->nkids = (int)o->kids.n;
            if (o->text.n) { o->node->text = str_n(fx, o->text.p, o->text.n); o->node->text_len = o->text.n; }
            --stack.n;
        }
    }
    xml_reader_free(fx->reader);
    fx->reader = NULL;
    return root;
}
/* }}} */

/* {{{ static join: a path written in a file, from that file's folder,
 * with the archives' backslashes */
static const char *join(Fxml *fx, const char *from, const char *rel)
{
    size_t dir = 0;
    for (size_t i = 0; from[i]; ++i) if (from[i] == '\\' || from[i] == '/') dir = i + 1;
    char *p = alloc(fx, dir + strlen(rel) + 1);
    memcpy(p, from, dir);
    strcpy(p + dir, rel);
    for (char *c = p; *c; ++c) if (*c == '/') *c = '\\';
    return p;
}
/* }}} */

/* {{{ templates: inherits split, the pieces in load order, the parent through them */
/* the next name of an inherits list from *s ("A, B"), into out; 0 at the end */
static int next_name(const char **s, char *out, size_t size)
{
    const char *p = *s;
    for (;;) {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == ',') ++p;
        if (!*p) { *s = p; return 0; }
        const char *q = p;
        while (*q && *q != ',') ++q;
        const char *e = q;
        while (e > p && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n')) --e;
        *s = q;
        if (e > p) {
            size_t n = (size_t)(e - p) < size - 1 ? (size_t)(e - p) : size - 1;
            memcpy(out, p, n);
            out[n] = 0;
            return 1;
        }
        p = q;
    }
}

typedef struct {
    int made;                             /* objects made under this top-level element */
    int pieces;                           /* templates applied under it, each time counted */
    int stopped;                          /* the limits were reached: one warning said so */
} Making;

typedef struct {
    const FxmlNode *in[FXML_MAX_INHERIT]; /* templates being applied, outermost first */
    int n;
} Applying;

static void apply(Fxml *fx, Making *mk, const FxmlNode *node, Applying *ap, Vec *pieces)
{
    if (mk->stopped) return;
    if (++mk->pieces > FXML_MAX_MADE) {
        char lim[64];
        snprintf(lim, sizeof lim, "%d templates applied for one element", FXML_MAX_MADE);
        warn_at(fx, FXML_W_TOO_BIG, node, describe(fx, node->tag, fxml_attr(node, "name")), lim);
        mk->stopped = 1;
        return;
    }
    const char *list = fxml_attr(node, "inherits");
    char nm[256];
    while (list && next_name(&list, nm, sizeof nm)) {
        const FxmlNode *t = map_get(&fx->templates, nm);
        if (!t) { warn_at(fx, FXML_W_NO_TEMPLATE, node, nm, NULL); continue; }
        int loop = t == node;
        for (int i = 0; i < ap->n && !loop; ++i) loop = ap->in[i] == t;
        if (loop) {
            const char *through = fxml_attr(node, "name");
            warn_at(fx, FXML_W_SELF_INHERIT, node, fxml_attr(t, "name"), through ? through : node->tag);
            continue;
        }
        if (ap->n >= FXML_MAX_INHERIT) {
            if (!mk->stopped) {
                char lim[64];
                snprintf(lim, sizeof lim, "%d templates inheriting templates", FXML_MAX_INHERIT);
                warn_at(fx, FXML_W_TOO_BIG, node, describe(fx, node->tag, fxml_attr(node, "name")), lim);
            }
            mk->stopped = 1;
            continue;
        }
        ap->in[ap->n++] = t;
        apply(fx, mk, t, ap, pieces);
        --ap->n;
    }
    vec_ptr(fx, pieces, node);
}

/* The parent a node names, itself or through its templates (the quirk:
 * only the last listed counts, unless corrected). `seen` holds the
 * templates already looked through, so a loop or a diamond is looked
 * through once. */
static const char *parent_through(Fxml *fx, const FxmlNode *node, Vec *seen, int depth)
{
    const char *own = fxml_attr(node, "parent");
    if (own) return own;
    const char *list = fxml_attr(node, "inherits");
    if (!list || depth >= FXML_MAX_INHERIT) return NULL;
    char nm[256];
    const FxmlNode *found[64];
    int n = 0;
    while (next_name(&list, nm, sizeof nm)) {
        const FxmlNode *t = map_get(&fx->templates, nm);
        if (n < 64) found[n++] = t;
        else { memmove(found, found + 1, 63 * sizeof *found); found[63] = t; }
    }
    for (int i = n - 1; i >= 0; --i) {
        const FxmlNode *t = found[i];
        int done = !t || t == node;
        for (size_t k = 0; k < seen->n && !done; ++k) done = AT(*seen, const FxmlNode *, k) == t;
        if (!done) {
            vec_ptr(fx, seen, t);
            const char *p = parent_through(fx, t, seen, depth + 1);
            if (p) return p;
        }
        if (!fx->opt.parent_from_any_template) break;   /* the original: the last listed, found or not */
    }
    return NULL;
}
/* }}} */

/* {{{ names: $parent worked out up the parent chain */
static const char *parent_base(const FxmlObject *parent, const char *parent_name)
{
    while (parent) {
        if (parent->name) return parent->name;
        parent_name = parent->parent ? NULL : parent->parent_name;
        parent = parent->parent;
    }
    return parent_name ? parent_name : "Top";
}
static const char *with_parent(Fxml *fx, const char *name, const FxmlObject *parent, const char *parent_name)
{
    if (!name || strncasecmp(name, "$parent", 7)) return name ? str(fx, name) : NULL;
    const char *base = parent_base(parent, parent_name);
    size_t a = strlen(base), b = strlen(name + 7);
    char *p = alloc(fx, a + b + 1);
    memcpy(p, base, a);
    memcpy(p + a, name + 7, b + 1);
    return p;
}
/* }}} */

/* {{{ static make: one object from its element, its templates applied */
typedef struct {
    const FxmlNode *node;
    const char *layer, *slot;
} KidNode;

static const char *point_of(const char *v)
{
    if (!v) return NULL;
    for (int i = 0; i < COUNT(POINTS); ++i) if (!strcasecmp(POINTS[i], v)) return POINTS[i];
    return NULL;
}
static double num(const char *v) { return v ? strtod(v, NULL) : 0; }

static FxmlObject *make(Fxml *fx, Making *mk, const FxmlNode *node, FxmlObject *up, const char *layer, const char *slot, int depth);

static void add_kid_node(Fxml *fx, Vec *kids, const FxmlNode *n, const char *layer, const char *slot)
{
    KidNode k = { n, layer, slot };
    vec_push(fx, kids, &k, sizeof k);
}

static void read_anchor(Fxml *fx, FxmlObject *o, const FxmlNode *a, Vec *anchors)
{
    const char *who = describe(fx, o->type, o->name);
    const char *pv = fxml_attr(a, "point"), *rpv = fxml_attr(a, "relativePoint");
    const char *point = point_of(pv), *rp = rpv ? point_of(rpv) : point;
    if (!point) { warn_at(fx, FXML_W_ANCHOR_POINT, a, who, pv ? pv : "(no point)"); return; }
    if (!rp) { warn_at(fx, FXML_W_ANCHOR_POINT, a, who, rpv); return; }
    FxmlAnchor an = { .point = point, .relative_point = rp, .from = a };
    const char *rel = fxml_attr(a, "relativeTo");
    if (rel) {
        const char *full = with_parent(fx, rel, o->parent, o->parent_name);
        const FxmlObject *r = map_get(&fx->names, full);
        if (r == o || (o->name && !strcmp(full, o->name))) { warn_at(fx, FXML_W_ANCHOR_SELF, a, who, NULL); return; }
        if (r) { an.relative = r; an.relative_name = r->name; }
        else if (fx->opt.frame_exists && fx->opt.frame_exists(fx->opt.ctx, full)) an.relative_name = full;
        else { warn_at(fx, FXML_W_ANCHOR_UNKNOWN, a, who, full); return; }
    } else {
        an.relative = o->parent;
        an.relative_name = o->parent_name;
    }
    an.x = num(fxml_attr(a, "x"));
    an.y = num(fxml_attr(a, "y"));
    for (int i = 0; i < a->nkids; ++i) {
        const FxmlNode *off = a->kids[i];
        if (strcasecmp(off->tag, "Offset")) continue;
        if (fxml_attr(off, "x")) an.x = num(fxml_attr(off, "x"));
        if (fxml_attr(off, "y")) an.y = num(fxml_attr(off, "y"));
        for (int j = 0; j < off->nkids; ++j)
            if (!strcasecmp(off->kids[j]->tag, "AbsDimension")) { an.x = num(fxml_attr(off->kids[j], "x")); an.y = num(fxml_attr(off->kids[j], "y")); }
    }
    for (size_t i = 0; i < anchors->n; ++i)
        if (AT(*anchors, FxmlAnchor, i).point == point) { AT(*anchors, FxmlAnchor, i) = an; return; }   /* the same point again: replaced */
    vec_push(fx, anchors, &an, sizeof an);
}

static FxmlObject *make(Fxml *fx, Making *mk, const FxmlNode *node, FxmlObject *up, const char *layer, const char *slot, int depth)
{
    if (mk->stopped) return NULL;
    if (depth >= FXML_MAX_NESTING || mk->made >= FXML_MAX_MADE) {
        char lim[64];
        if (depth >= FXML_MAX_NESTING) snprintf(lim, sizeof lim, "%d objects inside objects", FXML_MAX_NESTING);
        else snprintf(lim, sizeof lim, "%d objects made from one element", FXML_MAX_MADE);
        warn_at(fx, FXML_W_TOO_BIG, node, describe(fx, node->tag, fxml_attr(node, "name")), lim);
        mk->stopped = 1;
        return NULL;
    }
    ++mk->made;
    const char *type = node->tag;
    FxmlKind kind = kind_of(node->tag, &type);
    FxmlObject *o = alloc(fx, sizeof *o);
    o->kind = kind;
    o->type = type;
    o->up = up;
    o->layer = layer;
    o->slot = slot;
    o->node = node;
    o->taint = node->file->taint;
    if (up && is_true(fxml_attr(node, "virtual")))
        warn_at(fx, FXML_W_VIRTUAL_NESTED, node, describe(fx, type, fxml_attr(node, "name")), type);

    /* the pieces, then what each says, in that order */
    Vec pieces = { 0 }, sets = { 0 }, parts = { 0 }, anchor_nodes = { 0 }, scripts = { 0 }, kid_nodes = { 0 };
    Applying ap = { .n = 0 };
    apply(fx, mk, node, &ap, &pieces);
    for (size_t p = 0; p < pieces.n; ++p) {
        const FxmlNode *pc = AT(pieces, const FxmlNode *, p);
        for (int i = 0; i < pc->nattrs; ++i) {
            const FxmlAttr *a = &pc->attrs[i];
            if (!strcasecmp(a->name, "name") || !strcasecmp(a->name, "virtual") || !strcasecmp(a->name, "inherits") || !strcasecmp(a->name, "parent")) continue;
            FxmlSet s = { a->name, a->value, pc };
            size_t k = 0;
            while (k < sets.n && strcasecmp(AT(sets, FxmlSet, k).name, a->name)) ++k;
            if (k < sets.n) AT(sets, FxmlSet, k) = s;
            else vec_push(fx, &sets, &s, sizeof s);
        }
        for (int i = 0; i < pc->nkids; ++i) {
            const FxmlNode *k = pc->kids[i];
            const char *tag = k->tag;
            if (!strcasecmp(tag, "Frames") || !strcasecmp(tag, "ScrollChild")) {
                const char *as = strcasecmp(tag, "Frames") ? "ScrollChild" : NULL;
                for (int j = 0; j < k->nkids; ++j)
                    if (kind_of(k->kids[j]->tag, NULL)) add_kid_node(fx, &kid_nodes, k->kids[j], NULL, as);
                    else vec_ptr(fx, &parts, k->kids[j]);
            } else if (!strcasecmp(tag, "Layers")) {
                for (int j = 0; j < k->nkids; ++j) {
                    const FxmlNode *l = k->kids[j];
                    if (strcasecmp(l->tag, "Layer")) { vec_ptr(fx, &parts, l); continue; }
                    const char *level = fxml_attr(l, "level");
                    for (int r = 0; r < l->nkids; ++r)
                        if (kind_of(l->kids[r]->tag, NULL)) add_kid_node(fx, &kid_nodes, l->kids[r], level ? level : "ARTWORK", NULL);
                        else vec_ptr(fx, &parts, l->kids[r]);
                }
            } else if (!strcasecmp(tag, "Scripts")) {
                for (int j = 0; j < k->nkids; ++j) {
                    const FxmlNode *h = k->kids[j];
                    FxmlScript s = { h->tag, h->text, h->text_len, fxml_attr(h, "function"), h->text_line ? h->text_line : h->line, h };
                    size_t m = 0;
                    while (m < scripts.n && strcasecmp(AT(scripts, FxmlScript, m).handler, h->tag)) ++m;
                    if (m < scripts.n) AT(scripts, FxmlScript, m) = s;
                    else vec_push(fx, &scripts, &s, sizeof s);
                }
            } else if (!strcasecmp(tag, "Anchors")) {
                for (int j = 0; j < k->nkids; ++j)
                    if (!strcasecmp(k->kids[j]->tag, "Anchor")) vec_ptr(fx, &anchor_nodes, k->kids[j]);
                    else vec_ptr(fx, &parts, k->kids[j]);
            } else if (kind_of(tag, NULL)) {
                add_kid_node(fx, &kid_nodes, k, NULL, tag);   /* a named part: NormalTexture, ButtonText, an EditBox's FontString */
            } else vec_ptr(fx, &parts, k);
        }
    }
    o->pieces = pieces.p; o->npieces = (int)pieces.n;
    o->sets = sets.p; o->nsets = (int)sets.n;
    o->parts = parts.p; o->nparts = (int)parts.n;
    o->scripts = scripts.p; o->nscripts = (int)scripts.n;

    /* the parent: a region or font inside a frame is that frame's; a frame
     * names one, itself or through its templates, else the frame it is in */
    const char *named = NULL;
    if (kind == FXML_FRAME || !up) {
        Vec seen = { 0 };
        named = parent_through(fx, node, &seen, 0);
    }
    if (named) {
        const char *full = with_parent(fx, named, up, NULL);
        const FxmlObject *p = map_get(&fx->names, full);
        if (p) { o->parent = p; o->parent_name = p->name; }
        else if (fx->opt.frame_exists && fx->opt.frame_exists(fx->opt.ctx, full)) o->parent_name = full;
        else warn_at(fx, FXML_W_UNKNOWN_PARENT, node, describe(fx, type, fxml_attr(node, "name")), full);
    } else if (up) {
        o->parent = up;
        o->parent_name = up->name;
    }

    /* the name, known from here on */
    o->name = with_parent(fx, fxml_attr(node, "name"), o->parent, o->parent_name);
    if (o->name) map_put(fx, &fx->names, o->name, o);

    /* anchors, against what is made by now */
    Vec anchors = { 0 };
    for (size_t i = 0; i < anchor_nodes.n; ++i) read_anchor(fx, o, AT(anchor_nodes, const FxmlNode *, i), &anchors);
    o->anchors = anchors.p; o->nanchors = (int)anchors.n;
    const FxmlSet *all = NULL;
    for (int i = 0; i < o->nsets; ++i) if (!strcasecmp(o->sets[i].name, "setAllPoints")) all = &o->sets[i];
    if (all && is_true(all->value)) {
        if (anchor_nodes.n) warn_at(fx, FXML_W_ALL_POINTS, all->from, describe(fx, type, o->name), NULL);
        else o->set_all_points = 1;
    }

    /* the children, in load order */
    Vec kids = { 0 };
    for (size_t i = 0; i < kid_nodes.n; ++i) {
        const KidNode *k = &AT(kid_nodes, KidNode, i);
        FxmlObject *c = make(fx, mk, k->node, o, k->layer, k->slot, depth + 1);
        if (c) vec_ptr(fx, &kids, c);
    }
    o->kids = kids.p; o->nkids = (int)kids.n;
    return o;
}
/* }}} */

/* {{{ static load_file: one file's bytes, read and done */
static void step_add(Fxml *fx, FxmlStep s) { vec_push(fx, &fx->steps, &s, sizeof s); }

static WErr load_file(Fxml *fx, const char *path, const char *bytes, size_t len, const char *taint, int includes, WWhy *why);

static void do_top(Fxml *fx, const FxmlNode *n, int includes)
{
    const FxmlFile *f = n->file;
    if (!strcasecmp(n->tag, "Script")) {
        const char *file = fxml_attr(n, "file");
        if (file) step_add(fx, (FxmlStep){ .kind = FXML_STEP_SCRIPT_FILE, .path = join(fx, f->path, file), .node = n, .taint = f->taint, .line = n->line });
        size_t k = 0;
        while (k < n->text_len && strchr(" \t\n", n->text[k])) ++k;
        if (k < n->text_len)
            step_add(fx, (FxmlStep){ .kind = FXML_STEP_SCRIPT_TEXT, .text = n->text, .text_len = n->text_len, .line = n->text_line, .node = n, .taint = f->taint });
        return;
    }
    if (!strcasecmp(n->tag, "Include")) {
        const char *file = fxml_attr(n, "file");
        if (!file) { warn_at(fx, FXML_W_INCLUDE_NO_FILE, n, NULL, NULL); return; }
        const char *path = join(fx, f->path, file);
        if (includes >= FXML_MAX_INCLUDES) {
            char lim[16];
            snprintf(lim, sizeof lim, "%d", FXML_MAX_INCLUDES);
            warn_at(fx, FXML_W_INCLUDE_DEEP, n, path, lim);
            return;
        }
        WWhy why = { 0 };
        char *bytes = NULL;
        size_t len = 0;
        WErr e = fx->opt.open ? fx->opt.open(fx->opt.ctx, path, &bytes, &len, &why)
                              : w_fail(&why, W_ERR_NOT_FOUND, "no way to open files was given");
        if (e) { free(bytes); warn_at(fx, FXML_W_NOT_OPENED, n, path, why.text); return; }
        fx->bytes[includes] = bytes;
        load_file(fx, path, bytes, len, f->taint, includes + 1, &why);   /* its failure is in the log */
        fx->bytes[includes] = NULL;
        free(bytes);
        return;
    }
    const char *canon;
    FxmlKind kind = kind_of(n->tag, &canon);
    if (!kind) { warn_at(fx, FXML_W_TOP_UNKNOWN, n, n->tag, NULL); return; }
    if (is_true(fxml_attr(n, "virtual"))) {
        const char *name = fxml_attr(n, "name");
        if (!name) { warn_at(fx, FXML_W_VIRTUAL_UNNAMED, n, canon, NULL); return; }
        const FxmlNode *had = map_get(&fx->templates, name);
        if (!had || (is_trusted(f->taint) && !is_trusted(had->file->taint))) map_put(fx, &fx->templates, name, (void *)n);
        else warn_at(fx, FXML_W_TEMPLATE_TAKEN, n, name, NULL);
        return;
    }
    Making mk = { 0 };
    FxmlObject *o = make(fx, &mk, n, NULL, NULL, NULL, 0);
    if (o) step_add(fx, (FxmlStep){ .kind = FXML_STEP_OBJECT, .object = o, .node = n, .taint = f->taint, .line = n->line });
}

static WErr load_file(Fxml *fx, const char *path, const char *bytes, size_t len, const char *taint, int includes, WWhy *why)
{
    FxmlFile *file = alloc(fx, sizeof *file);
    file->path = str(fx, path ? path : "(unnamed)");
    file->taint = str(fx, taint ? taint : "");
    WWhy bad = { 0 };
    FxmlNode *root = read_file(fx, file, bytes, len, &bad);
    if (!root) {
        warn(fx, FXML_W_NOT_XML, file->path, 0, file->path, bad.text);
        *why = bad;
        return bad.code;
    }
    if (strcasecmp(root->tag, "Ui")) {
        warn(fx, FXML_W_NOT_UI, file->path, root->line, file->path, root->tag);
        return w_fail(why, W_ERR_CORRUPT, "%s: its top element is <%s>, not <Ui>", file->path, root->tag);
    }
    file->root = root;
    for (int i = 0; i < root->nkids; ++i) do_top(fx, root->kids[i], includes);
    return W_OK;
}
/* }}} */

/* {{{ the public calls: each sets where running out of memory lands */
#define ENTER(fx, why, fail_value)                                                  \
    if (!(fx)->entered) {                                                           \
        if (setjmp((fx)->out)) {                                                    \
            (fx)->entered = 0;                                                      \
            xml_reader_free((fx)->reader); (fx)->reader = NULL;                     \
            for (int b_ = 0; b_ <= FXML_MAX_INCLUDES; ++b_) { free((fx)->bytes[b_]); (fx)->bytes[b_] = NULL; } \
            w_fail((why), W_ERR_FULL, "no memory left for the interface's XML");    \
            return fail_value;                                                      \
        }                                                                           \
    }                                                                               \
    ++(fx)->entered

#define LEAVE(fx) (--(fx)->entered)

Fxml *fxml_new(const FxmlOptions *opt, WWhy *why)
{
    Fxml *fx = calloc(1, sizeof *fx);
    if (!fx) { w_fail(why, W_ERR_FULL, "no memory for the interface's XML"); return NULL; }
    if (opt) fx->opt = *opt;
    fx->templates.nocase = 1;
    return fx;
}

void fxml_free(Fxml *fx)
{
    if (!fx) return;
    for (Block *b = fx->blocks, *next; b; b = next) { next = b->next; free(b); }
    free(fx);
}

WErr fxml_load(Fxml *fx, const char *path, const char *bytes, size_t len, const char *taint, WWhy *why)
{
    ENTER(fx, why, W_ERR_FULL);
    WErr e = load_file(fx, path, bytes, len, taint, 0, why);
    LEAVE(fx);
    return e;
}

WErr fxml_create(Fxml *fx, const char *type, const char *name, const char *parent, const char *inherits, const char *taint, const FxmlObject **out, WWhy *why)
{
    const char *canon;
    *out = NULL;
    if (!type || !kind_of(type, &canon)) return w_fail(why, W_ERR_REFUSED, "%s isn't a type of frame, region or font made from XML", type ? type : "(no type)");
    ENTER(fx, why, W_ERR_FULL);
    FxmlFile *file = alloc(fx, sizeof *file);
    file->path = "(CreateFrame)";
    file->taint = str(fx, taint ? taint : "");
    FxmlNode *n = alloc(fx, sizeof *n);
    FxmlAttr *a = alloc(fx, 3 * sizeof *a);
    int na = 0;
    if (name) a[na++] = (FxmlAttr){ "name", str(fx, name) };
    if (parent) a[na++] = (FxmlAttr){ "parent", str(fx, parent) };
    if (inherits) a[na++] = (FxmlAttr){ "inherits", str(fx, inherits) };
    *n = (FxmlNode){ .tag = canon, .attrs = a, .nattrs = na, .text = "", .file = file };
    Making mk = { 0 };
    *out = make(fx, &mk, n, NULL, NULL, NULL, 0);
    LEAVE(fx);
    return W_OK;
}

const FxmlNode *fxml_template(const Fxml *fx, const char *name) { return map_get(&fx->templates, name); }
const FxmlObject *fxml_find(const Fxml *fx, const char *name) { return map_get(&fx->names, name); }
int fxml_step_count(const Fxml *fx) { return (int)fx->steps.n; }
const FxmlStep *fxml_step_at(const Fxml *fx, int i) { return i >= 0 && (size_t)i < fx->steps.n ? &AT(fx->steps, FxmlStep, i) : NULL; }
int fxml_warning_count(const Fxml *fx) { return (int)fx->warnings.n; }
const FxmlWarning *fxml_warning_at(const Fxml *fx, int i) { return i >= 0 && (size_t)i < fx->warnings.n ? &AT(fx->warnings, FxmlWarning, i) : NULL; }

void fxml_warning_line(const FxmlWarning *w, char *out, size_t size)
{
    if (!size) return;
    if (w->line) snprintf(out, size, "%s:%d %s", w->file, w->line, w->text);
    else snprintf(out, size, "%s %s", w->file, w->text);
}
/* }}} */
