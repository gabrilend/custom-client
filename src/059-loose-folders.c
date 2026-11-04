/*
 * 059-loose-folders.c (issues 104, 105a) - see 059-loose-folders.h
 *
 * For a general audience: three small pieces. A folder walk that lists
 * every file under a name folded to small letters; a reader for Lua
 * tables of plain data (the manifest's language, and a converted map's
 * map.lua, read without running any Lua); and the manifest's fields,
 * each held to its type and range.
 */
#include "059-loose-folders.h"
#include "013-errors.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define FS_DEPTH_MAX   32                 /* folders inside folders, at most (a link looping back stops here) */
#define LUA_DEPTH_MAX  32                 /* tables inside tables, at most */
#define MANIFEST_MAX   ((size_t)16 << 20) /* bytes a manifest may hold: far past any real pack's */
#define MANIFEST_NOTE  "a manifest is a Lua table of plain data: docs/012, \"The pack manifest\""
#define DATA_NOTE      "read as Lua data: strings, numbers, true, false, nil and tables, nothing else"

/* {{{ fs_normalise */
int fs_normalise(const char *in, char *out, size_t n)
{
    size_t o = 0;
    const char *p = in;
    while (*p) {
        while (*p == '/' || *p == '\\') ++p;                       /* leading and doubled slashes */
        const char *part = p;
        while (*p && *p != '/' && *p != '\\') ++p;
        size_t len = (size_t)(p - part);
        if (len == 0 || (len == 1 && part[0] == '.')) continue;   /* "./" parts */
        if (o && o + 1 < n) out[o++] = '/';
        else if (o) return 1;
        for (size_t i = 0; i < len; ++i) {
            if (o + 1 >= n) return 1;
            out[o++] = (char)tolower((unsigned char)part[i]);
        }
    }
    if (o == 0 || o >= n) return 1;
    out[o] = '\0';
    return 0;
}
/* }}} */

/* {{{ the folder index
 * Every file under the root, folded name and full path, sorted by the
 * folded name so a lookup is a binary search. */
typedef struct { char *key; char *real; } FsFile;
struct FsIndex { FsFile *files; int count, cap; };

void fs_index_free(FsIndex *ix)
{
    if (!ix) return;
    for (int i = 0; i < ix->count; ++i) { free(ix->files[i].key); free(ix->files[i].real); }
    free(ix->files);
    free(ix);
}

static int file_order(const void *a, const void *b) { return strcmp(((const FsFile *)a)->key, ((const FsFile *)b)->key); }

/* {{{ static walk: adds the files under `real` (whose folded name so far is `key`, "" at the root) */
static WErr walk(FsIndex *ix, const char *real, const char *key, int depth, WWhy *why)
{
    if (depth > FS_DEPTH_MAX)
        return w_fail(why, W_ERR_REFUSED, "%s: folders nest more than %d deep (a link looping back to a folder above it?)", real, FS_DEPTH_MAX);
    DIR *d = opendir(real);
    if (!d) return w_fail(why, W_ERR_IO, "can't read the folder %s (%s)", real, strerror(errno));
    WErr rc = W_OK;
    struct dirent *e;
    while (rc == W_OK && (e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        size_t rl = strlen(real) + 1 + strlen(e->d_name) + 1, kl = strlen(key) + 1 + strlen(e->d_name) + 1;
        char *rp = malloc(rl), *kp = malloc(kl);
        snprintf(rp, rl, "%s/%s", real, e->d_name);
        snprintf(kp, kl, "%s%s%s", key, *key ? "/" : "", e->d_name);
        for (char *c = kp; *c; ++c) *c = (char)tolower((unsigned char)*c);
        struct stat st;
        if (stat(rp, &st) != 0) {
            rc = w_fail(why, W_ERR_IO, "can't look at %s (%s)", rp, strerror(errno));
        } else if (S_ISDIR(st.st_mode)) {
            rc = walk(ix, rp, kp, depth + 1, why);
        } else if (S_ISREG(st.st_mode)) {
            if (ix->count == ix->cap) {
                ix->cap = ix->cap ? ix->cap * 2 : 64;
                ix->files = realloc(ix->files, sizeof *ix->files * (size_t)ix->cap);
            }
            ix->files[ix->count++] = (FsFile){ kp, rp };
            continue;                                   /* kept: not freed below */
        }                                               /* anything else (a socket, a broken link's target) is not a file: left out */
        free(rp);
        free(kp);
    }
    closedir(d);
    return rc;
}
/* }}} */

WErr fs_index_folder(const char *root, FsIndex **out, WWhy *why)
{
    *out = NULL;
    struct stat st;
    if (stat(root, &st) != 0 || !S_ISDIR(st.st_mode))
        return w_fail(why, W_ERR_NOT_FOUND, "%s is not a folder", root);
    FsIndex *ix = calloc(1, sizeof *ix);
    WErr rc = walk(ix, root, "", 0, why);
    if (rc) { fs_index_free(ix); return rc; }
    qsort(ix->files, (size_t)ix->count, sizeof *ix->files, file_order);
    for (int i = 1; i < ix->count; ++i)
        if (!strcmp(ix->files[i - 1].key, ix->files[i].key)) {
            w_fail(why, W_ERR_REFUSED, "%s and %s differ only by capitals; a request for %s would have to pick one, so neither is taken: rename one",
                   ix->files[i - 1].real, ix->files[i].real, ix->files[i].key);
            fs_index_free(ix);
            return why->code;
        }
    *out = ix;
    return W_OK;
}

const char *fs_index_find(const FsIndex *ix, const char *logical)
{
    char key[1024];
    if (!ix || fs_normalise(logical, key, sizeof key)) return NULL;
    FsFile probe = { key, NULL };
    const FsFile *f = bsearch(&probe, ix->files, (size_t)ix->count, sizeof *ix->files, file_order);
    return f ? f->real : NULL;
}

int fs_index_count(const FsIndex *ix) { return ix ? ix->count : 0; }
const char *fs_index_at(const FsIndex *ix, int i, const char **real)
{
    if (!ix || i < 0 || i >= ix->count) return NULL;
    if (real) *real = ix->files[i].real;
    return ix->files[i].key;
}
/* }}} */

/* {{{ the kinds' names */
static const char *const KIND_NAMES[ASSET_KIND_COUNT] = {
    [ASSET_MODEL] = "model", [ASSET_TEXTURE] = "texture", [ASSET_ANIMATION] = "animation", [ASSET_PARTICLE] = "particle",
    [ASSET_SOUND] = "sound", [ASSET_FONT] = "font", [ASSET_INTERFACE] = "interface", [ASSET_TABLE] = "table",
    [ASSET_GROUND] = "ground",
};
const char *asset_kind_name(AssetKind kind) { return (kind >= 0 && kind < ASSET_KIND_COUNT) ? KIND_NAMES[kind] : "(not a kind)"; }
int asset_kind_from_name(const char *name)
{
    for (int k = 0; k < ASSET_KIND_COUNT; ++k) if (!strcmp(KIND_NAMES[k], name)) return k;
    return -1;
}
/* }}} */

/* {{{ the arena: a manifest's strings and tables, freed together */
typedef struct Chunk { struct Chunk *next; size_t used, cap; } Chunk;
static void *arena_alloc(Chunk **a, size_t n)
{
    n = (n + 15) & ~(size_t)15;
    if (!*a || (*a)->used + n > (*a)->cap) {
        size_t cap = n > 65536 ? n : 65536;
        Chunk *c = malloc(sizeof(Chunk) + 16 + cap);
        c->next = *a; c->used = 0; c->cap = cap;
        *a = c;
    }
    void *p = (unsigned char *)(*a) + ((sizeof(Chunk) + 15) & ~(size_t)15) + (*a)->used;
    (*a)->used += n;
    memset(p, 0, n);
    return p;
}
static char *arena_str(Chunk **a, const char *s, size_t len)
{
    char *p = arena_alloc(a, len + 1);
    memcpy(p, s, len);
    p[len] = '\0';
    return p;
}
static void arena_free(Chunk *a) { while (a) { Chunk *n = a->next; free(a); a = n; } }
/* }}} */

/* {{{ Lua data: the reader
 * `return` and one value: a table, a string (short, with escapes, or
 * long, in [[ ]] or [=[ ]=]), a number (with a sign), true, false or nil.
 * A table's items are `name = value`, `["key"] = value` or a value alone
 * (the next in its list), parted by commas or semicolons. Comments are
 * -- to the line's end, or --[[ ]]. Anything else (a name standing for a
 * value, a call, arithmetic) is refused with its line: a manifest is
 * data, never a program. */
typedef enum { LV_NIL = LUA_DATA_NIL, LV_BOOL = LUA_DATA_BOOL, LV_NUM = LUA_DATA_NUMBER, LV_STR = LUA_DATA_TEXT, LV_TABLE = LUA_DATA_TABLE } LvType;
typedef struct LvItem LvItem;
typedef struct LuaValue Lv;
struct LuaValue {
    LvType      t;
    int         line;
    int         b;
    double      n;
    const char *s;
    LvItem     *first;                /* a table's items, in order */
    int         count, listed;        /* items; of them, those with no key */
};
struct LvItem { const char *key; int key_line; Lv *val; LvItem *next; };

typedef struct {
    const char *s, *end;
    int         line;
    const char *where;
    WWhy       *why;
    Chunk     **arena;
    int         depth;
    const char *note;                 /* said after every refusal: what the text is read as */
} Lex;

static const char *const RESERVED[] = { "and", "break", "do", "else", "elseif", "end", "false", "for", "function", "goto", "if",
                                        "in", "local", "nil", "not", "or", "repeat", "return", "then", "true", "until", "while", NULL };
static int reserved(const char *w, size_t n)
{
    for (int i = 0; RESERVED[i]; ++i) if (strlen(RESERVED[i]) == n && !strncmp(RESERVED[i], w, n)) return 1;
    return 0;
}

static WErr bad(Lex *x, const char *what)
{
    return w_fail(x->why, W_ERR_CORRUPT, "%s line %d: %s (%s)", x->where, x->line, what, x->note);
}

/* {{{ static long_open: at "[", "[=", "[==" ... "[": the level (number of =), or -1 */
static int long_open(const Lex *x, const char *p)
{
    if (p >= x->end || *p != '[') return -1;
    int level = 0;
    ++p;
    while (p < x->end && *p == '=') { ++level; ++p; }
    return (p < x->end && *p == '[') ? level : -1;
}
/* }}} */

/* {{{ static long_body: reads a long bracket's text (the opening already
 * known to be at x->s with `level`); out may be NULL (a comment) */
static WErr long_body(Lex *x, int level, const char **out, size_t *len)
{
    int start_line = x->line;
    x->s += level + 2;
    if (x->s < x->end && *x->s == '\r') ++x->s;                 /* a newline straight after the opening is not part of it */
    if (x->s < x->end && *x->s == '\n') { ++x->s; ++x->line; }
    const char *from = x->s;
    while (x->s < x->end) {
        if (*x->s == ']') {
            const char *p = x->s + 1;
            int l = 0;
            while (p < x->end && *p == '=') { ++l; ++p; }
            if (l == level && p < x->end && *p == ']') {
                if (out) {
                    if (memchr(from, '\0', (size_t)(x->s - from))) return bad(x, "a string holding a zero byte");
                    *out = arena_str(x->arena, from, (size_t)(x->s - from));
                    *len = (size_t)(x->s - from);
                }
                x->s = p + 1;
                return W_OK;
            }
        }
        if (*x->s == '\n') ++x->line;
        ++x->s;
    }
    x->line = start_line;
    return bad(x, "a long string or comment that opens here never closes");
}
/* }}} */

/* {{{ static skip: spaces, newlines and comments */
static WErr skip(Lex *x)
{
    for (;;) {
        while (x->s < x->end && isspace((unsigned char)*x->s)) { if (*x->s == '\n') ++x->line; ++x->s; }
        if (x->s + 1 < x->end && x->s[0] == '-' && x->s[1] == '-') {
            x->s += 2;
            int level = long_open(x, x->s);
            if (level >= 0) { WErr rc = long_body(x, level, NULL, NULL); if (rc) return rc; }
            else while (x->s < x->end && *x->s != '\n') ++x->s;
            continue;
        }
        return W_OK;
    }
}
/* }}} */

/* {{{ static short_string: "..." or '...', with Lua's escapes */
static WErr short_string(Lex *x, const char **out)
{
    char quote = *x->s++;
    size_t cap = 64, n = 0;
    char *buf = malloc(cap);
    WErr rc = W_OK;
    for (;;) {
        if (x->s >= x->end || *x->s == '\n') { rc = bad(x, "a string that doesn't close on its line"); break; }
        char c = *x->s++;
        if (c == quote) break;
        if (c == '\\') {
            if (x->s >= x->end) { rc = bad(x, "a string ending in a backslash"); break; }
            char e = *x->s++;
            int v = -1;
            switch (e) {
            case 'a': v = '\a'; break;  case 'b': v = '\b'; break;  case 'f': v = '\f'; break;
            case 'n': v = '\n'; break;  case 'r': v = '\r'; break;  case 't': v = '\t'; break;
            case 'v': v = '\v'; break;  case '\\': v = '\\'; break; case '"': v = '"'; break;
            case '\'': v = '\''; break; case '\n': v = '\n'; ++x->line; break;
            case 'x':
                if (x->s + 2 <= x->end && isxdigit((unsigned char)x->s[0]) && isxdigit((unsigned char)x->s[1])) {
                    char hex[3] = { x->s[0], x->s[1], 0 };
                    v = (int)strtol(hex, NULL, 16);
                    x->s += 2;
                }
                break;
            default:
                if (isdigit((unsigned char)e)) {
                    v = e - '0';
                    for (int i = 0; i < 2 && x->s < x->end && isdigit((unsigned char)*x->s); ++i) v = v * 10 + (*x->s++ - '0');
                    if (v > 255) v = -1;
                }
            }
            if (v < 0) { rc = bad(x, "a string with an escape Lua doesn't know"); break; }
            if (v == 0) { rc = bad(x, "a string holding a zero byte"); break; }
            c = (char)v;
        }
        if (n + 1 >= cap) buf = realloc(buf, cap *= 2);
        buf[n++] = c;
    }
    if (!rc) *out = arena_str(x->arena, buf, n);
    free(buf);
    return rc;
}
/* }}} */

static WErr value(Lex *x, Lv **out);

/* {{{ static table: { items } */
static WErr table(Lex *x, Lv *t)
{
    if (++x->depth > LUA_DEPTH_MAX) return bad(x, "tables nested too deep");
    ++x->s;                                                         /* the { */
    LvItem **tail = &t->first;
    for (;;) {
        WErr rc = skip(x);
        if (rc) return rc;
        if (x->s >= x->end) return bad(x, "a table that never closes");
        if (*x->s == '}') { ++x->s; break; }
        LvItem *it = arena_alloc(x->arena, sizeof *it);
        it->key_line = x->line;
        if (*x->s == '[' && long_open(x, x->s) < 0) {               /* ["key"] = value */
            ++x->s;
            Lv *k;
            if ((rc = skip(x)) || (rc = value(x, &k))) return rc;
            if (k->t != LV_STR) return bad(x, "a key in [ ] that isn't a string (this reader takes only names and strings as keys)");
            if ((rc = skip(x))) return rc;
            if (x->s >= x->end || *x->s != ']') return bad(x, "a [ key without its ]");
            ++x->s;
            if ((rc = skip(x))) return rc;
            if (x->s >= x->end || *x->s != '=') return bad(x, "a [\"key\"] without = after it");
            ++x->s;
            it->key = k->s;
        } else if (isalpha((unsigned char)*x->s) || *x->s == '_') { /* name = value, or true/false/nil alone */
            const char *w = x->s, *p = x->s;
            while (p < x->end && (isalnum((unsigned char)*p) || *p == '_')) ++p;
            const char *save = x->s; int save_line = x->line;
            x->s = p;
            if ((rc = skip(x))) return rc;
            if (x->s < x->end && *x->s == '=' && !(x->s + 1 < x->end && x->s[1] == '=')) {
                if (reserved(w, (size_t)(p - w))) return bad(x, "a key that is one of Lua's own words");
                ++x->s;
                it->key = arena_str(x->arena, w, (size_t)(p - w));
            } else { x->s = save; x->line = save_line; }
        }
        if ((rc = skip(x)) || (rc = value(x, &it->val))) return rc;
        if (it->key)
            for (LvItem *o = t->first; o; o = o->next)
                if (o->key && !strcmp(o->key, it->key)) {
                    x->line = it->key_line;
                    return bad(x, "a key given twice in one table");
                }
        if (!it->key) ++t->listed;
        ++t->count;
        *tail = it;
        tail = &it->next;
        if ((rc = skip(x))) return rc;
        if (x->s < x->end && (*x->s == ',' || *x->s == ';')) { ++x->s; continue; }
        if (x->s < x->end && *x->s == '}') { ++x->s; break; }
        return bad(x, "a table item not followed by a comma or the table's }");
    }
    --x->depth;
    return W_OK;
}
/* }}} */

/* {{{ static value */
static WErr value(Lex *x, Lv **out)
{
    Lv *v = arena_alloc(x->arena, sizeof *v);
    *out = v;
    v->line = x->line;
    if (x->s >= x->end) return bad(x, "the text ends where a value should be");
    char c = *x->s;
    if (c == '{') { v->t = LV_TABLE; return table(x, v); }
    if (c == '"' || c == '\'') { v->t = LV_STR; return short_string(x, &v->s); }
    int level = long_open(x, x->s);
    if (level >= 0) { size_t len; v->t = LV_STR; return long_body(x, level, &v->s, &len); }
    if (isdigit((unsigned char)c) || c == '-' || (c == '.' && x->s + 1 < x->end && isdigit((unsigned char)x->s[1]))) {
        int negative = 0;
        if (c == '-') {
            negative = 1;
            ++x->s;
            WErr rc = skip(x);
            if (rc) return rc;
            if (x->s >= x->end || !(isdigit((unsigned char)*x->s) || *x->s == '.')) return bad(x, "a minus sign that isn't before a number");
        }
        const char *p = x->s;
        while (p < x->end && (isalnum((unsigned char)*p) || *p == '.' ||
               ((*p == '+' || *p == '-') && (p[-1] == 'e' || p[-1] == 'E' || p[-1] == 'p' || p[-1] == 'P')))) ++p;
        char num[64];
        size_t n = (size_t)(p - x->s);
        if (n >= sizeof num) return bad(x, "a number too long to be one");
        memcpy(num, x->s, n);
        num[n] = '\0';
        char *end;
        double d = strtod(num, &end);
        if (*end || !isfinite(d) || !strpbrk(num, "0123456789")) return bad(x, "something that starts like a number but isn't one");
        v->t = LV_NUM;
        v->n = negative ? -d : d;
        x->s = p;
        return W_OK;
    }
    if (isalpha((unsigned char)c) || c == '_') {
        const char *p = x->s;
        while (p < x->end && (isalnum((unsigned char)*p) || *p == '_')) ++p;
        size_t n = (size_t)(p - x->s);
        if (n == 4 && !strncmp(x->s, "true", 4))  { v->t = LV_BOOL; v->b = 1; x->s = p; return W_OK; }
        if (n == 5 && !strncmp(x->s, "false", 5)) { v->t = LV_BOOL; v->b = 0; x->s = p; return W_OK; }
        if (n == 3 && !strncmp(x->s, "nil", 3))   { v->t = LV_NIL; x->s = p; return W_OK; }
        return bad(x, "a name where a value should be (this reader takes data only: no names, calls or sums)");
    }
    return bad(x, "a character that starts no value");
}
/* }}} */

/* {{{ static lua_data: `return value` and nothing after */
static WErr lua_data(Lex *x, Lv **out)
{
    WErr rc = skip(x);
    if (rc) return rc;
    if (x->end - x->s < 6 || strncmp(x->s, "return", 6) || (x->s + 6 < x->end && (isalnum((unsigned char)x->s[6]) || x->s[6] == '_')))
        return bad(x, "the file doesn't start with return");
    x->s += 6;
    if ((rc = skip(x)) || (rc = value(x, out)) || (rc = skip(x))) return rc;
    if (x->s < x->end && *x->s == ';') { ++x->s; if ((rc = skip(x))) return rc; }
    if (x->s < x->end) return bad(x, "more after the returned value");
    return W_OK;
}
/* }}} */
/* }}} */

/* {{{ the manifest's fields */
typedef struct { PackManifest m; Chunk *arena; } ManifestHeld;   /* the handle's hidden half: m first, so a PackManifest * is one */

static const Lv *field(const Lv *t, const char *key)
{
    for (const LvItem *it = t->first; it; it = it->next) if (it->key && !strcmp(it->key, key)) return it->val;
    return NULL;
}
static const char *TYPE_NAMES[] = { "nil", "true or false", "a number", "a string", "a table" };

/* every key of `t` is one of `allowed`, and nothing in it is listed without a key */
static WErr only_keys(const Lv *t, const char *const *allowed, const char *where, const char *what, WWhy *why)
{
    for (const LvItem *it = t->first; it; it = it->next) {
        if (!it->key) return w_fail(why, W_ERR_REFUSED, "%s line %d: %s holds a value with no name; each of its fields is name = value", where, it->val->line, what);
        int ok = 0;
        for (int i = 0; allowed[i]; ++i) if (!strcmp(allowed[i], it->key)) ok = 1;
        if (!ok) return w_fail(why, W_ERR_REFUSED, "%s line %d: %s has a field %s that manifest format 1 doesn't have (misspelt?)", where, it->key_line, what, it->key);
    }
    return W_OK;
}

static WErr want(const Lv *v, LvType t, const char *where, const char *what, const char *key, WWhy *why)
{
    if (v->t != t)
        return w_fail(why, W_ERR_REFUSED, "%s line %d: %s's %s is %s; it must be %s", where, v->line, what, key, TYPE_NAMES[v->t], TYPE_NAMES[t]);
    return W_OK;
}

/* a string field: required (0 when absent fails) or "" when absent */
static WErr text_field(const Lv *t, const char *key, int required, const char **out, const char *where, const char *what, WWhy *why)
{
    const Lv *v = field(t, key);
    if (!v) {
        if (required) return w_fail(why, W_ERR_REFUSED, "%s line %d: %s has no %s; manifest format 1 needs one", where, t->line, what, key);
        *out = "";
        return W_OK;
    }
    WErr rc = want(v, LV_STR, where, what, key, why);
    if (!rc) *out = v->s;
    return rc;
}

static int is_date(const char *s)
{
    if (strlen(s) != 10 || s[4] != '-' || s[7] != '-') return 0;
    for (int i = 0; i < 10; ++i) if (i != 4 && i != 7 && !isdigit((unsigned char)s[i])) return 0;
    int month = (s[5] - '0') * 10 + (s[6] - '0'), day = (s[8] - '0') * 10 + (s[9] - '0');
    return month >= 1 && month <= 12 && day >= 1 && day <= 31;
}

static WErr date_field(const Lv *t, const char *key, char out[11], const char *where, const char *what, WWhy *why)
{
    const char *s;
    WErr rc = text_field(t, key, 1, &s, where, what, why);
    if (rc) return rc;
    if (!is_date(s)) return w_fail(why, W_ERR_REFUSED, "%s line %d: %s's %s \"%s\" is not a date written YYYY-MM-DD", where, field(t, key)->line, what, key, s);
    memcpy(out, s, 11);
    return W_OK;
}

static WErr word_field(const Lv *t, const char *key, const char *const *words, int *out, const char *where, const char *what, WWhy *why)
{
    const char *s;
    WErr rc = text_field(t, key, 1, &s, where, what, why);
    if (rc) return rc;
    for (int i = 0; words[i]; ++i) if (!strcmp(words[i], s)) { *out = i; return W_OK; }
    char list[200] = "";
    for (int i = 0; words[i]; ++i) { strncat(list, i ? ", " : "", sizeof list - strlen(list) - 1); strncat(list, words[i], sizeof list - strlen(list) - 1); }
    return w_fail(why, W_ERR_REFUSED, "%s line %d: %s's %s \"%s\" is not one of: %s", where, field(t, key)->line, what, key, s, list);
}

static const char *const ROUTES[]   = { "search", "generate", "hand", "restyle", NULL };
static const char *const LINEAGES[] = { "derived", "independent", NULL };
static const char *const KINDS[]    = { "model", "texture", "animation", "particle", "sound", "font", "interface", "table", "ground", NULL };
static const char *const TOP_KEYS[]   = { "format", "name", "entries", NULL };
static const char *const ENTRY_KEYS[] = { "replaces", "file", "kind", "id", "route", "lineage", "licence", "generation",
                                          "scores", "seal", "transcript", "wc3", NULL };
static const char *const GEN_KEYS[]   = { "prompt", "seed", "workflow", "models", NULL };
static const char *const SCORE_KEYS[] = { "score", "date", "by", NULL };
static const char *const SEAL_KEYS[]  = { "build", "date", NULL };

/* {{{ static entry: one entry's fields, into `e` */
static WErr entry(const Lv *t, int index, PackEntry *e, Chunk **arena, const char *where, WWhy *why)
{
    char what[400];
    snprintf(what, sizeof what, "entry %d", index + 1);
    WErr rc = want(t, LV_TABLE, where, "entries", what, why);
    if (rc || (rc = only_keys(t, ENTRY_KEYS, where, what, why))) return rc;
    e->line = t->line;
    if ((rc = text_field(t, "replaces", 1, &e->replaces, where, what, why))) return rc;
    snprintf(what, sizeof what, "entry %d (replaces \"%.300s\")", index + 1, e->replaces);
    char key[1024];
    if (fs_normalise(e->replaces, key, sizeof key))
        return w_fail(why, W_ERR_REFUSED, "%s line %d: %s: the path is empty or too long", where, t->line, what);
    e->key = arena_str(arena, key, strlen(key));
    int n;
    if ((rc = text_field(t, "file", 1, &e->file, where, what, why))) return rc;
    if ((rc = word_field(t, "kind", KINDS, &n, where, what, why))) return rc;
    e->kind = (AssetKind)n;
    const char *id;
    if ((rc = text_field(t, "id", 1, &id, where, what, why))) return rc;
    int hex = strlen(id) == 64;
    for (int i = 0; hex && i < 64; ++i) hex = isdigit((unsigned char)id[i]) || (id[i] >= 'a' && id[i] <= 'f');
    if (!hex) return w_fail(why, W_ERR_REFUSED, "%s line %d: %s's id is not a SHA-256 written as 64 small hex digits (as sha256sum prints it)", where, field(t, "id")->line, what);
    memcpy(e->id, id, 65);
    if ((rc = word_field(t, "route", ROUTES, &n, where, what, why))) return rc;
    e->route = (PackRoute)n;
    if ((rc = word_field(t, "lineage", LINEAGES, &n, where, what, why))) return rc;
    e->lineage = (PackLineage)n;
    if ((rc = text_field(t, "licence", e->route == PACK_ROUTE_SEARCH, &e->licence, where, what, why))) return rc;
    if ((rc = text_field(t, "transcript", 0, &e->transcript, where, what, why))) return rc;

    const Lv *g = field(t, "generation");
    e->prompt = e->workflow = "";
    if (!g && e->route == PACK_ROUTE_GENERATE)
        return w_fail(why, W_ERR_REFUSED, "%s line %d: %s was generated but has no generation record (prompt, seed, workflow, models)", where, t->line, what);
    if (g) {
        char gw[440];
        snprintf(gw, sizeof gw, "%s's generation", what);
        if ((rc = want(g, LV_TABLE, where, what, "generation", why)) || (rc = only_keys(g, GEN_KEYS, where, gw, why))) return rc;
        e->has_generation = 1;
        if ((rc = text_field(g, "prompt", 0, &e->prompt, where, gw, why)) || (rc = text_field(g, "workflow", 0, &e->workflow, where, gw, why))) return rc;
        const Lv *seed = field(g, "seed");
        if (seed) {
            if ((rc = want(seed, LV_STR, where, gw, "seed (written as a string of digits: 64 bits don't fit Lua's numbers)", why))) return rc;
            int digits = seed->s[0] != '\0' && strlen(seed->s) <= 20;
            for (const char *p = seed->s; digits && *p; ++p) digits = isdigit((unsigned char)*p);
            errno = 0;
            unsigned long long s = digits ? strtoull(seed->s, NULL, 10) : 0;
            if (!digits || errno == ERANGE)
                return w_fail(why, W_ERR_REFUSED, "%s line %d: %s's seed \"%s\" is not a whole number from 0 to 18446744073709551615", where, seed->line, gw, seed->s);
            e->seed = (uint64_t)s;
            e->has_seed = 1;
        }
        const Lv *models = field(g, "models");
        if (models) {
            if ((rc = want(models, LV_TABLE, where, gw, "models", why))) return rc;
            if (models->listed != models->count)
                return w_fail(why, W_ERR_REFUSED, "%s line %d: %s's models is a list of names, with no keys", where, models->line, gw);
            e->models = arena_alloc(arena, sizeof *e->models * (size_t)(models->count ? models->count : 1));
            for (const LvItem *it = models->first; it; it = it->next) {
                if ((rc = want(it->val, LV_STR, where, gw, "models' each name", why))) return rc;
                e->models[e->nmodels++] = it->val->s;
            }
        }
    }

    const Lv *scores = field(t, "scores");
    if (scores) {
        if ((rc = want(scores, LV_TABLE, where, what, "scores", why))) return rc;
        if (scores->listed != scores->count)
            return w_fail(why, W_ERR_REFUSED, "%s line %d: %s's scores is a list, oldest first, with no keys", where, scores->line, what);
        PackScore *list = arena_alloc(arena, sizeof *list * (size_t)(scores->count ? scores->count : 1));
        int i = 0;
        for (const LvItem *it = scores->first; it; it = it->next, ++i) {
            char sw[460];
            snprintf(sw, sizeof sw, "%s's score %d", what, i + 1);
            const Lv *s = it->val;
            if ((rc = want(s, LV_TABLE, where, what, "scores' each record", why)) || (rc = only_keys(s, SCORE_KEYS, where, sw, why))) return rc;
            const Lv *num = field(s, "score");
            if (!num) return w_fail(why, W_ERR_REFUSED, "%s line %d: %s has no score", where, s->line, sw);
            if ((rc = want(num, LV_NUM, where, sw, "score", why))) return rc;
            if (num->n < 0 || num->n > 1)
                return w_fail(why, W_ERR_REFUSED, "%s line %d: %s is %g; a similarity score runs from 0 (the original) to 1 (unrelated)", where, num->line, sw, num->n);
            list[i].score = num->n;
            if ((rc = date_field(s, "date", list[i].date, where, sw, why)) || (rc = text_field(s, "by", 0, &list[i].by, where, sw, why))) return rc;
            if (i > 0 && strcmp(list[i].date, list[i - 1].date) < 0)
                return w_fail(why, W_ERR_REFUSED, "%s line %d: %s (%s) is dated before the one above it (%s); scores are listed oldest first, so the last is the latest",
                              where, s->line, sw, list[i].date, list[i - 1].date);
        }
        e->scores = list;
        e->nscores = scores->count;
    }

    const Lv *seal = field(t, "seal");
    if (seal && seal->t == LV_TABLE) {
        char sw[440];
        snprintf(sw, sizeof sw, "%s's seal", what);
        if ((rc = only_keys(seal, SEAL_KEYS, where, sw, why))) return rc;
        const Lv *build = field(seal, "build");
        if (!build) return w_fail(why, W_ERR_REFUSED, "%s line %d: %s names no build: a seal says which stock client it was checked in", where, seal->line, sw);
        if ((rc = want(build, LV_NUM, where, sw, "build", why))) return rc;
        if (build->n < 1 || build->n > 4294967295.0 || build->n != floor(build->n))
            return w_fail(why, W_ERR_REFUSED, "%s line %d: %s's build %g is not a client build number", where, build->line, sw, build->n);
        e->sealed = 1;
        e->seal_build = (uint32_t)build->n;
        if ((rc = date_field(seal, "date", e->seal_date, where, sw, why))) return rc;
    } else if (seal && !(seal->t == LV_BOOL && !seal->b)) {
        return w_fail(why, W_ERR_REFUSED, "%s line %d: %s's seal must be false or { build = ..., date = ... } (the stock client's build and the day it was checked: world-edit-to-execute W05c)",
                      where, seal->line, what);
    }
    const Lv *wc3 = field(t, "wc3");
    if (wc3 && (rc = want(wc3, LV_TABLE, where, what, "wc3", why))) return rc;
    return W_OK;
}
/* }}} */

void pack_manifest_free(PackManifest *m)
{
    if (!m) return;
    arena_free(((ManifestHeld *)m)->arena);
    free(m);
}

WErr pack_manifest_parse(const char *text, size_t len, const char *where, PackManifest **out, WWhy *why)
{
    *out = NULL;
    ManifestHeld *h = calloc(1, sizeof *h);
    Lex x = { text, text + len, 1, where, why, &h->arena, 0, MANIFEST_NOTE };
    Lv *top;
    WErr rc = lua_data(&x, &top);
    if (!rc && top->t != LV_TABLE) rc = w_fail(why, W_ERR_REFUSED, "%s: it returns %s; a manifest returns a table", where, TYPE_NAMES[top->t]);
    if (!rc) rc = only_keys(top, TOP_KEYS, where, "the manifest", why);
    const Lv *format = rc ? NULL : field(top, "format");
    if (!rc && !format) rc = w_fail(why, W_ERR_REFUSED, "%s: no format; this reader knows format = 1", where);
    if (!rc && format->t != LV_NUM) rc = want(format, LV_NUM, where, "the manifest", "format", why);
    if (!rc && format->n != 1)
        rc = w_fail(why, W_ERR_UNSUPPORTED, "%s line %d: manifest format %g; this reader knows format 1 only (a newer world-edit-to-execute or client wrote it?)",
                    where, format->line, format->n);
    if (!rc) { h->m.format = 1; rc = text_field(top, "name", 1, &h->m.name, where, "the manifest", why); }
    const Lv *entries = rc ? NULL : field(top, "entries");
    if (!rc && !entries) rc = w_fail(why, W_ERR_REFUSED, "%s: no entries; a pack with nothing in it says entries = {}", where);
    if (!rc) rc = want(entries, LV_TABLE, where, "the manifest", "entries", why);
    if (!rc && entries->listed != entries->count)
        rc = w_fail(why, W_ERR_REFUSED, "%s line %d: entries is a list of entries, with no keys", where, entries->line);
    if (!rc) {
        h->m.entries = arena_alloc(&h->arena, sizeof *h->m.entries * (size_t)(entries->count ? entries->count : 1));
        int i = 0;
        for (const LvItem *it = entries->first; !rc && it; it = it->next, ++i) {
            rc = entry(it->val, i, &h->m.entries[i], &h->arena, where, why);
            for (int j = 0; !rc && j < i; ++j)
                if (!strcmp(h->m.entries[j].key, h->m.entries[i].key))
                    rc = w_fail(why, W_ERR_REFUSED, "%s: entries %d (line %d) and %d (line %d) both replace %s; one pack answers a path once",
                                where, j + 1, h->m.entries[j].line, i + 1, h->m.entries[i].line, h->m.entries[i].replaces);
        }
        h->m.count = i;
    }
    if (rc) { pack_manifest_free(&h->m); return rc; }
    *out = &h->m;
    return W_OK;
}

WErr pack_manifest_read(const char *path, PackManifest **out, WWhy *why)
{
    *out = NULL;
    FILE *f = fopen(path, "rb");
    if (!f) return w_fail(why, errno == ENOENT ? W_ERR_NOT_FOUND : W_ERR_IO, "no manifest at %s (%s)", path, strerror(errno));
    struct stat st;
    if (fstat(fileno(f), &st) != 0) { fclose(f); return w_fail(why, W_ERR_IO, "can't look at %s (%s)", path, strerror(errno)); }
    if ((size_t)st.st_size > MANIFEST_MAX) {
        fclose(f);
        return w_fail(why, W_ERR_REFUSED, "%s is %lld bytes; no manifest is over %zu", path, (long long)st.st_size, MANIFEST_MAX);
    }
    size_t want_len = (size_t)st.st_size;
    char *text = malloc(want_len + 1);
    size_t len = fread(text, 1, want_len, f);
    int err = ferror(f) || len != want_len;
    fclose(f);
    WErr rc = err ? w_fail(why, W_ERR_IO, "can't read %s (%zu of %zu bytes)", path, len, want_len)
                  : pack_manifest_parse(text, len, path, out, why);
    free(text);
    return rc;
}

double pack_entry_latest(const PackEntry *e) { return e->nscores ? e->scores[e->nscores - 1].score : -1; }
/* }}} */

/* {{{ Lua data, for any file of it (a converted map's map.lua) */
struct LuaData { Chunk *arena; const Lv *top; };

WErr lua_data_parse(const char *text, size_t len, const char *where, LuaData **out, WWhy *why)
{
    *out = NULL;
    LuaData *d = calloc(1, sizeof *d);
    Lex x = { text, text + len, 1, where, why, &d->arena, 0, DATA_NOTE };
    Lv *top;
    WErr rc = lua_data(&x, &top);
    if (rc) { lua_data_free(d); return rc; }
    d->top = top;
    *out = d;
    return W_OK;
}

WErr lua_data_read(const char *path, size_t most, LuaData **out, WWhy *why)
{
    *out = NULL;
    FILE *f = fopen(path, "rb");
    if (!f) return w_fail(why, errno == ENOENT ? W_ERR_NOT_FOUND : W_ERR_IO, "no %s (%s)", path, strerror(errno));
    struct stat st;
    if (fstat(fileno(f), &st) != 0) { fclose(f); return w_fail(why, W_ERR_IO, "can't look at %s (%s)", path, strerror(errno)); }
    if (!S_ISREG(st.st_mode)) { fclose(f); return w_fail(why, W_ERR_REFUSED, "%s is not a file", path); }
    if ((size_t)st.st_size > most) { fclose(f); return w_fail(why, W_ERR_REFUSED, "%s is %lld bytes; this file is never over %zu", path, (long long)st.st_size, most); }
    size_t want_len = (size_t)st.st_size;
    char *text = malloc(want_len + 1);
    size_t len = fread(text, 1, want_len, f);
    int err = ferror(f) || len != want_len;
    fclose(f);
    WErr rc = err ? w_fail(why, W_ERR_IO, "can't read %s (%zu of %zu bytes)", path, len, want_len) : lua_data_parse(text, len, path, out, why);
    free(text);
    return rc;
}

void lua_data_free(LuaData *d)
{
    if (!d) return;
    arena_free(d->arena);
    free(d);
}

const LuaValue *lua_data_top(const LuaData *d) { return d->top; }
LuaDataType lua_value_type(const LuaValue *v) { return v ? (LuaDataType)v->t : LUA_DATA_NIL; }
const char *lua_type_name(LuaDataType t) { return (t >= LUA_DATA_NIL && t <= LUA_DATA_TABLE) ? TYPE_NAMES[t] : "(not a type)"; }
int lua_value_line(const LuaValue *v) { return v ? v->line : 0; }
double lua_value_number(const LuaValue *v) { return v && v->t == LV_NUM ? v->n : 0; }
int lua_value_true(const LuaValue *v) { return v && v->t == LV_BOOL && v->b; }
const char *lua_value_text(const LuaValue *v) { return v && v->t == LV_STR ? v->s : NULL; }
const LuaValue *lua_value_field(const LuaValue *t, const char *key) { return t && t->t == LV_TABLE ? field(t, key) : NULL; }
int lua_value_listed(const LuaValue *t) { return t && t->t == LV_TABLE ? t->listed : 0; }
const LuaValue *lua_value_item(const LuaValue *t, int i)
{
    if (!t || t->t != LV_TABLE || i < 1) return NULL;
    int n = 0;
    for (const LvItem *it = t->first; it; it = it->next)
        if (!it->key && ++n == i) return it->val;
    return NULL;
}
/* }}} */
