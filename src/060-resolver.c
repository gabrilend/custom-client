/*
 * 060-resolver.c (issue 105a) - see 060-resolver.h
 *
 * For a general audience: at the start every folder is listed (059) and
 * every pack's manifest read and checked against its folder; then each
 * request is a lookup in those lists, in order, and the archives after
 * them. What answered is counted once per asset, under a lock, since the
 * reading-ahead thread may ask too.
 */
#include "060-resolver.h"
#include "013-errors.h"

#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

/* {{{ the resolver's state */
typedef struct {
    char             folder[600];
    FsIndex         *ix;
    PackManifest    *m;
    const PackEntry **by_key;          /* the manifest's entries sorted by folded path */
    const char     **real;             /* each one's file on disk, beside it */
} Pack;

typedef struct { char folder[600]; FsIndex *ix; } Loose;

struct Resolver {
    Pack           packs[RESOLVER_FOLDERS_MAX];   int npacks;
    Loose          server[RESOLVER_FOLDERS_MAX];  int nserver;
    Loose          overlays[RESOLVER_FOLDERS_MAX]; int noverlays;
    MpqChain      *chain;
    int            strict;
    double         distinctness;
    FILE          *log;
    int            trace;
    /* the built-in placeholders, made at open */
    unsigned char *checker, *box, *box_skin, *beep;
    size_t         checker_len, box_len, box_skin_len, beep_len;
    /* the session's counts, and the assets already counted ("k/path", k the kind's number) */
    pthread_mutex_t lock;
    AssetCounts    counts;
    char         **seen;  size_t seen_cap, seen_count;
};
/* }}} */

/* {{{ asset_source_name */
static const char *const SOURCE_NAMES[ASSET_SOURCE_COUNT] = {
    [ASSET_FROM_PACK] = "pack", [ASSET_FROM_SERVER] = "server", [ASSET_FROM_OVERLAY] = "overlay",
    [ASSET_FROM_BLIZZARD] = "blizzard", [ASSET_FROM_PLACEHOLDER] = "placeholder",
};
const char *asset_source_name(AssetSource s) { return (s >= 0 && s < ASSET_SOURCE_COUNT) ? SOURCE_NAMES[s] : "(not a place)"; }
/* }}} */

/* {{{ the placeholders
 * Little-endian writers into a zeroed buffer, then each file laid out as
 * its reader expects (017-blp.c, 022-m2.c). */
static void put16(unsigned char *p, uint32_t v) { p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8); }
static void put32(unsigned char *p, uint32_t v) { put16(p, v); put16(p + 2, v >> 16); }
static void putf(unsigned char *p, float f) { uint32_t v; memcpy(&v, &f, 4); put32(p, v); }

/* {{{ static make_checker: BLP2, plain BGRA, 8 x 8, squares of 2 x 2,
 * magenta and black; one level (header 148 bytes, palette 1024 unused,
 * pixels from 1172) */
#define CHECKER_SIDE 8
static unsigned char *make_checker(size_t *len)
{
    size_t at = 148 + 1024, n = CHECKER_SIDE * CHECKER_SIDE;
    *len = at + n * 4;
    unsigned char *s = calloc(1, *len);
    memcpy(s, "BLP2", 4);
    put32(s + 4, 1);                          /* type: plain, not JPEG */
    s[8] = 3; s[9] = 8; s[10] = 0; s[11] = 0; /* encoding plain, 8 alpha bits, no mipmaps */
    put32(s + 12, CHECKER_SIDE); put32(s + 16, CHECKER_SIDE);
    put32(s + 20, (uint32_t)at); put32(s + 84, (uint32_t)(n * 4));
    for (size_t i = 0; i < n; ++i) {
        int x = (int)(i % CHECKER_SIDE), y = (int)(i / CHECKER_SIDE);
        int magenta = ((x / 2) + (y / 2)) % 2 == 0;
        unsigned char *p = s + at + i * 4;    /* blue, green, red, alpha */
        p[0] = magenta ? 255 : 0; p[1] = 0; p[2] = magenta ? 255 : 0; p[3] = 255;
    }
    return s;
}
/* }}} */

/* {{{ static make_box: M2 version 264, a box a yard on each side standing
 * on the ground at its middle (x and y from -0.5 to 0.5, z from 0 to 1),
 * its eight corners facing out; and its .skin: one piece, twelve
 * triangles, one unit with no picture */
static const int BOX_TRIS[36] = { 0,2,1, 1,2,3,  4,5,6, 5,7,6,  0,1,4, 1,5,4,  2,6,3, 3,6,7,  0,4,2, 2,4,6,  1,3,5, 3,7,5 };
static unsigned char *make_box(size_t *len)
{
    const char name[] = "W placeholder box";
    size_t header = 304, name_at = header, verts_at = (name_at + sizeof name + 15) & ~(size_t)15;
    *len = verts_at + 8 * 48;
    unsigned char *s = calloc(1, *len);
    memcpy(s, "MD20", 4);
    put32(s + 4, 264);
    put32(s + 8, sizeof name); put32(s + 12, (uint32_t)name_at);
    memcpy(s + name_at, name, sizeof name);
    put32(s + 60, 8); put32(s + 64, (uint32_t)verts_at);
    put32(s + 68, 1);                         /* views (skin files): one */
    for (int i = 0; i < 8; ++i) {
        float x = (i & 1) ? 0.5f : -0.5f, y = (i & 2) ? 0.5f : -0.5f, z = (i & 4) ? 1.0f : 0.0f;
        unsigned char *v = s + verts_at + (size_t)i * 48;
        putf(v, x); putf(v + 4, y); putf(v + 8, z);
        v[12] = 255;                          /* all its weight on bone 0 (there are none: a static box) */
        float nz = z - 0.5f, k = 1.0f / sqrtf(x * x + y * y + nz * nz);
        putf(v + 20, x * k); putf(v + 24, y * k); putf(v + 28, nz * k);
        putf(v + 32, x + 0.5f); putf(v + 36, 1.0f - z);
    }
    return s;
}
static unsigned char *make_box_skin(size_t *len)
{
    size_t points_at = 48, tris_at = points_at + 8 * 2, pieces_at = (tris_at + 36 * 2 + 15) & ~(size_t)15, units_at = pieces_at + 48;
    *len = units_at + 24;
    unsigned char *s = calloc(1, *len);
    memcpy(s, "SKIN", 4);
    put32(s + 4, 8);  put32(s + 8, (uint32_t)points_at);
    put32(s + 12, 36); put32(s + 16, (uint32_t)tris_at);
    put32(s + 28, 1); put32(s + 32, (uint32_t)pieces_at);
    put32(s + 36, 1); put32(s + 40, (uint32_t)units_at);
    for (int i = 0; i < 8; ++i) put16(s + points_at + (size_t)i * 2, (uint32_t)i);
    for (int i = 0; i < 36; ++i) put16(s + tris_at + (size_t)i * 2, (uint32_t)BOX_TRIS[i]);
    unsigned char *p = s + pieces_at;         /* the piece: points 0 to 7, triangle corners 0 to 35 */
    put16(p + 6, 8); put16(p + 10, 36);
    unsigned char *u = s + units_at;          /* the unit: piece 0, no colour, one layer */
    put16(u + 8, 0xFFFF); put16(u + 14, 1);
    return s;
}
/* }}} */

/* {{{ static make_beep: WAV, 16-bit mono at 22050 a second, 0.2 s of
 * 880 Hz at a third of full loudness, faded in and out over 5 ms so it
 * doesn't click */
static unsigned char *make_beep(size_t *len)
{
    const uint32_t rate = 22050, n = rate / 5, fade = rate / 200;
    *len = 44 + (size_t)n * 2;
    unsigned char *s = calloc(1, *len);
    memcpy(s, "RIFF", 4); put32(s + 4, (uint32_t)*len - 8); memcpy(s + 8, "WAVEfmt ", 8);
    put32(s + 16, 16); put16(s + 20, 1); put16(s + 22, 1); put32(s + 24, rate); put32(s + 28, rate * 2);
    put16(s + 32, 2); put16(s + 34, 16);
    memcpy(s + 36, "data", 4); put32(s + 40, n * 2);
    for (uint32_t i = 0; i < n; ++i) {
        double edge = i < fade ? (double)i / fade : (n - 1 - i < fade ? (double)(n - 1 - i) / fade : 1.0);
        double v = sin(2 * 3.14159265358979 * 880.0 * i / rate) * (32767.0 / 3) * edge;
        put16(s + 44 + (size_t)i * 2, (uint32_t)(uint16_t)(int16_t)lrint(v));
    }
    return s;
}
/* }}} */

/* {{{ static ends_with: `name` ends in `tail`, without regard to case */
static int ends_with(const char *name, const char *tail)
{
    size_t n = strlen(name), t = strlen(tail);
    if (t > n) return 0;
    for (size_t i = 0; i < t; ++i)
        if ((name[n - t + i] | 0x20) != (tail[i] | 0x20)) return 0;
    return 1;
}
/* }}} */

/* {{{ static placeholder: the built-in bytes for `kind` (and, for a box, its file by name), and their description for the warning */
static const unsigned char *placeholder(const Resolver *r, AssetKind kind, const char *key, size_t *len, const char **what)
{
    switch (kind) {
    case ASSET_TEXTURE: case ASSET_INTERFACE:
        *what = "the magenta checker"; *len = r->checker_len; return r->checker;
    case ASSET_MODEL: case ASSET_PARTICLE:
        if (ends_with(key, ".skin")) { *what = "the box's skin"; *len = r->box_skin_len; return r->box_skin; }
        *what = "a box"; *len = r->box_len; return r->box;
    case ASSET_SOUND:
        *what = "a beep"; *len = r->beep_len; return r->beep;
    case ASSET_ANIMATION:
        *what = "nothing (the resting pose)"; *len = 0; return NULL;
    case ASSET_FONT: default:
        *what = "nothing (the client's built-in font)"; *len = 0; return NULL;
    }
}
/* }}} */
/* }}} */

/* {{{ resolver_place_from_settings */
static WErr split_names(const char *list, const char *under, char out[][600], int *n, const char *setting, WWhy *why)
{
    *n = 0;
    const char *p = list;
    while (*p) {
        const char *comma = strchr(p, ',');
        size_t len = comma ? (size_t)(comma - p) : strlen(p);
        if (len) {
            if (*n == RESOLVER_FOLDERS_MAX) return w_fail(why, W_ERR_FULL, "%s names more than %d folders", setting, RESOLVER_FOLDERS_MAX);
            snprintf(out[*n], 600, "%s/%.*s", under, (int)len, p);
            ++*n;
        }
        p += len + (comma ? 1 : 0);
    }
    return W_OK;
}
WErr resolver_place_from_settings(const Settings *st, const char *assets_dir, MpqChain *chain, ResolverPlace *out, WWhy *why)
{
    memset(out, 0, sizeof *out);
    char under[600];
    snprintf(under, sizeof under, "%s/packs", assets_dir);
    WErr rc = split_names(settings_text(st, "assetPacks"), under, out->packs, &out->npacks, "assetPacks", why);
    if (rc) return rc;
    snprintf(under, sizeof under, "%s/overlays", assets_dir);
    if ((rc = split_names(settings_text(st, "assetOverlays"), under, out->overlays, &out->noverlays, "assetOverlays", why))) return rc;
    out->chain = chain;
    out->strict = (int)settings_num(st, "assetsStrict");
    out->distinctness = settings_num(st, "assetsDistinctness");
    return W_OK;
}
/* }}} */

/* {{{ resolver_open, resolver_free */
void resolver_free(Resolver *r)
{
    if (!r) return;
    for (int i = 0; i < r->npacks; ++i) {
        fs_index_free(r->packs[i].ix);
        pack_manifest_free(r->packs[i].m);
        free(r->packs[i].by_key);
        free(r->packs[i].real);
    }
    for (int i = 0; i < r->nserver; ++i) fs_index_free(r->server[i].ix);
    for (int i = 0; i < r->noverlays; ++i) fs_index_free(r->overlays[i].ix);
    for (size_t i = 0; i < r->seen_cap; ++i) free(r->seen[i]);
    free(r->seen);
    free(r->checker); free(r->box); free(r->box_skin); free(r->beep);
    pthread_mutex_destroy(&r->lock);
    free(r);
}

static int entry_order(const void *a, const void *b)
{
    return strcmp((*(const PackEntry *const *)a)->key, (*(const PackEntry *const *)b)->key);
}

/* {{{ static open_pack: index the folder, read the manifest, find each entry's file */
static WErr open_pack(Resolver *r, Pack *p, const char *folder, WWhy *why)
{
    snprintf(p->folder, sizeof p->folder, "%s", folder);
    WErr rc = fs_index_folder(folder, &p->ix, why);
    if (rc) return rc == W_ERR_NOT_FOUND ? w_fail(why, rc, "the pack folder %s is not there (assetPacks names it)", folder) : rc;
    const char *manifest = fs_index_find(p->ix, "manifest.lua");
    if (!manifest)
        return w_fail(why, W_ERR_NOT_FOUND, "the pack %s has no manifest.lua: a pack lists each file's provenance (docs/012); a folder of files without one is an overlay", folder);
    if ((rc = pack_manifest_read(manifest, &p->m, why))) return rc;
    int n = p->m->count;
    p->by_key = malloc(sizeof *p->by_key * (size_t)(n ? n : 1));
    p->real = malloc(sizeof *p->real * (size_t)(n ? n : 1));
    for (int i = 0; i < n; ++i) p->by_key[i] = &p->m->entries[i];
    qsort(p->by_key, (size_t)n, sizeof *p->by_key, entry_order);
    for (int i = 0; i < n; ++i) {
        p->real[i] = fs_index_find(p->ix, p->by_key[i]->file);
        if (!p->real[i])
            return w_fail(why, W_ERR_NOT_FOUND, "%s line %d: the file %s (replacing %s) is not in the pack's folder %s",
                          manifest, p->by_key[i]->line, p->by_key[i]->file, p->by_key[i]->replaces, folder);
    }
    /* files no entry names are never served (each pack file needs its
     * provenance): said once at the start, by name, so a forgotten entry is seen */
    int unlisted = 0;
    char names[400] = "";
    for (int f = 0; f < fs_index_count(p->ix); ++f) {
        const char *real;
        fs_index_at(p->ix, f, &real);
        if (real == manifest) continue;
        int named = 0;
        for (int i = 0; i < n && !named; ++i) named = p->real[i] == real;
        if (named) continue;
        if (unlisted < 5) {
            strncat(names, unlisted ? ", " : "", sizeof names - strlen(names) - 1);
            strncat(names, fs_index_at(p->ix, f, NULL), sizeof names - strlen(names) - 1);
        }
        ++unlisted;
    }
    if (unlisted)
        fprintf(r->log, "assets: pack \"%s\" (%s): %d file%s with no entry in its manifest, never served: %s%s\n",
                p->m->name, folder, unlisted, unlisted == 1 ? "" : "s", names, unlisted > 5 ? ", ..." : "");
    return W_OK;
}
/* }}} */

WErr resolver_open(const ResolverPlace *place, Resolver **out, WWhy *why)
{
    *out = NULL;
    if (place->npacks < 0 || place->npacks > RESOLVER_FOLDERS_MAX || place->nserver < 0 || place->nserver > RESOLVER_FOLDERS_MAX
        || place->noverlays < 0 || place->noverlays > RESOLVER_FOLDERS_MAX)
        return w_fail(why, W_ERR_REFUSED, "a resolver takes 0 to %d folders of each kind (packs %d, server %d, overlays %d)",
                      RESOLVER_FOLDERS_MAX, place->npacks, place->nserver, place->noverlays);
    if (!(place->distinctness >= 0 && place->distinctness <= 1))
        return w_fail(why, W_ERR_REFUSED, "the distinctness threshold %g is outside 0 to 1", place->distinctness);
    Resolver *r = calloc(1, sizeof *r);
    pthread_mutex_init(&r->lock, NULL);
    r->chain = place->chain;
    r->strict = place->strict;
    r->distinctness = place->distinctness;
    r->log = place->log ? place->log : stderr;
    r->trace = place->trace;
    r->checker = make_checker(&r->checker_len);
    r->box = make_box(&r->box_len);
    r->box_skin = make_box_skin(&r->box_skin_len);
    r->beep = make_beep(&r->beep_len);
    WErr rc = W_OK;
    for (int i = 0; !rc && i < place->npacks; ++i, ++r->npacks) rc = open_pack(r, &r->packs[i], place->packs[i], why);
    for (int k = 0; k < 2 && !rc; ++k) {
        Loose *list = k ? r->overlays : r->server;
        int want = k ? place->noverlays : place->nserver, *have = k ? &r->noverlays : &r->nserver;
        for (int i = 0; !rc && i < want; ++i, ++*have) {
            const char *folder = k ? place->overlays[i] : place->server[i];
            snprintf(list[i].folder, sizeof list[i].folder, "%s", folder);
            rc = fs_index_folder(folder, &list[i].ix, why);
            if (rc == W_ERR_NOT_FOUND) rc = w_fail(why, rc, "the %s folder %s is not there", k ? "overlay" : "server's assets", folder);
        }
    }
    if (rc) { resolver_free(r); return rc; }
    r->seen_cap = 256;
    r->seen = calloc(r->seen_cap, sizeof *r->seen);
    *out = r;
    return W_OK;
}
/* }}} */

/* {{{ static first_time: 1 the first time `kind` and `key` are asked for (the lock held) */
static uint64_t text_hash(const char *s) { uint64_t h = 1469598103934665603ull; while (*s) h = (h ^ (unsigned char)*s++) * 1099511628211ull; return h; }
static int first_time(Resolver *r, AssetKind kind, const char *key)
{
    size_t n = strlen(key) + 4;
    char *k = malloc(n);
    snprintf(k, n, "%d/%s", (int)kind, key);
    if ((r->seen_count + 1) * 2 > r->seen_cap) {           /* kept at most half full: double it */
        size_t cap = r->seen_cap * 2;
        char **bigger = calloc(cap, sizeof *bigger);
        for (size_t i = 0; i < r->seen_cap; ++i)
            if (r->seen[i]) {
                size_t j = text_hash(r->seen[i]) & (cap - 1);
                while (bigger[j]) j = (j + 1) & (cap - 1);
                bigger[j] = r->seen[i];
            }
        free(r->seen);
        r->seen = bigger;
        r->seen_cap = cap;
    }
    size_t j = text_hash(k) & (r->seen_cap - 1);
    while (r->seen[j]) {
        if (!strcmp(r->seen[j], k)) { free(k); return 0; }
        j = (j + 1) & (r->seen_cap - 1);
    }
    r->seen[j] = k;
    ++r->seen_count;
    return 1;
}
/* }}} */

/* {{{ static count: the answer counted (once per asset), the lock taken here */
static void count(Resolver *r, AssetKind kind, const char *key, AssetSource src, const PackEntry *e)
{
    pthread_mutex_lock(&r->lock);
    if (src == ASSET_FROM_PLACEHOLDER) ++r->counts.placeholder_answers;
    if (first_time(r, kind, key)) {
        ++r->counts.by[kind][src];
        if (e && pack_entry_latest(e) >= r->distinctness) ++r->counts.distinct[kind];
    }
    pthread_mutex_unlock(&r->lock);
}
/* }}} */

/* {{{ static read_file: a whole file into a malloc'd buffer */
static WErr read_file(const char *path, unsigned char **data, size_t *len, WWhy *why)
{
    FILE *f = fopen(path, "rb");
    if (!f) return w_fail(why, W_ERR_IO, "can't open %s, though it was there at the start", path);
    if (fseeko(f, 0, SEEK_END) != 0) { fclose(f); return w_fail(why, W_ERR_IO, "can't find the end of %s", path); }
    off_t size = ftello(f);
    rewind(f);
    unsigned char *s = malloc(size > 0 ? (size_t)size : 1);
    size_t got = size > 0 ? fread(s, 1, (size_t)size, f) : 0;
    fclose(f);
    if (size < 0 || got != (size_t)size) { free(s); return w_fail(why, W_ERR_IO, "can't read %s (%zu of %lld bytes)", path, got, (long long)size); }
    *data = s;
    *len = (size_t)size;
    return W_OK;
}
/* }}} */

/* {{{ asset_resolve */
void asset_answer_free(AssetAnswer *a)
{
    if (!a) return;
    free(a->data);
    memset(a, 0, sizeof *a);
}

WErr asset_resolve(Resolver *r, const char *logical, AssetKind kind, AssetAnswer *out, WWhy *why)
{
    memset(out, 0, sizeof *out);
    char key[1024];
    if (kind < 0 || kind >= ASSET_KIND_COUNT) return w_fail(why, W_ERR_REFUSED, "asked for %s as kind %d, which is not a kind", logical ? logical : "(nothing)", (int)kind);
    if (!logical || fs_normalise(logical, key, sizeof key)) return w_fail(why, W_ERR_REFUSED, "asked for a %s with an empty or too long name", asset_kind_name(kind));
    const char *kname = asset_kind_name(kind);

    /* 1. packs, highest priority first */
    for (int i = 0; i < r->npacks; ++i) {
        Pack *p = &r->packs[i];
        PackEntry probe = { .key = key };
        const PackEntry *pp = &probe, **hit = bsearch(&pp, p->by_key, (size_t)p->m->count, sizeof *p->by_key, entry_order);
        if (!hit) continue;
        const PackEntry *e = *hit;
        const char *real = p->real[hit - p->by_key];
        if (e->kind != kind)
            return w_fail(why, W_ERR_REFUSED, "%s was asked for as a %s, but the pack \"%s\" (%s, line %d) lists it as a %s: one of the two is wrong",
                          logical, kname, p->m->name, p->folder, e->line, asset_kind_name(e->kind));
        WErr rc = read_file(real, &out->data, &out->len, why);
        if (rc) return rc;
        count(r, kind, key, ASSET_FROM_PACK, e);
        out->source = ASSET_FROM_PACK;
        out->from = p->folder;
        out->entry = e;
        if (r->trace) {
            double s = pack_entry_latest(e);
            fprintf(r->log, "assets: %s %s <- pack \"%s\" (%s), score %s%.2f%s%s\n", kname, logical, p->m->name, real,
                    s < 0 ? "none" : "", s < 0 ? 0.0 : s, s >= r->distinctness ? ", replaced" : ", still borrowed", e->sealed ? ", sealed" : "");
        }
        return W_OK;
    }
    /* 2 and 3. the server's assets, then overlays */
    for (int k = 0; k < 2; ++k) {
        const Loose *list = k ? r->overlays : r->server;
        int n = k ? r->noverlays : r->nserver;
        AssetSource src = k ? ASSET_FROM_OVERLAY : ASSET_FROM_SERVER;
        for (int i = 0; i < n; ++i) {
            const char *real = fs_index_find(list[i].ix, key);
            if (!real) continue;
            WErr rc = read_file(real, &out->data, &out->len, why);
            if (rc) return rc;
            count(r, kind, key, src, NULL);
            out->source = src;
            out->from = list[i].folder;
            if (r->trace) fprintf(r->log, "assets: %s %s <- %s (%s)\n", kname, logical, asset_source_name(src), real);
            return W_OK;
        }
    }
    /* 4. the archives (their names take backslashes; case doesn't matter to them) */
    if (r->chain) {
        char name[1024];
        snprintf(name, sizeof name, "%s", key);
        for (char *c = name; *c; ++c) if (*c == '/') *c = '\\';
        const char *from = NULL;
        size_t len = 0;
        unsigned char *got = mpq_chain_read(r->chain, name, &len, &from);
        if (got) {
            count(r, kind, key, ASSET_FROM_BLIZZARD, NULL);
            out->data = got;
            out->len = len;
            out->source = ASSET_FROM_BLIZZARD;
            out->from = from;
            if (r->trace) fprintf(r->log, "assets: %s %s <- blizzard (%s)\n", kname, logical, from ? from : "the archives' memory");
            return W_OK;
        }
    }
    /* 5. the placeholder, said every time */
    count(r, kind, key, ASSET_FROM_PLACEHOLDER, NULL);
    const char *what;
    size_t len;
    const unsigned char *bytes = placeholder(r, kind, key, &len, &what);
    char where[200];
    snprintf(where, sizeof where, "%d pack%s, %d server folder%s, %d overlay%s%s", r->npacks, r->npacks == 1 ? "" : "s",
             r->nserver, r->nserver == 1 ? "" : "s", r->noverlays, r->noverlays == 1 ? "" : "s", r->chain ? " or the archives" : ", no archives open");
    if (r->strict) {
        fprintf(r->log, "assets: ERROR: the %s %s is in none of %s; strict mode (assetsStrict) refuses a placeholder\n", kname, logical, where);
        return w_fail(why, W_ERR_NOT_FOUND, "the %s %s is in none of %s, and strict mode refuses a placeholder", kname, logical, where);
    }
    fprintf(r->log, "assets: WARNING: the %s %s is in none of %s; %s stands in\n", kname, logical, where, what);
    out->data = malloc(len ? len : 1);
    if (len) memcpy(out->data, bytes, len);
    out->len = len;
    out->source = ASSET_FROM_PLACEHOLDER;
    out->from = "built in";
    return W_OK;
}
/* }}} */

/* {{{ asset_counts, resolver_report, resolver_exit_status */
void asset_counts(Resolver *r, AssetCounts *out)
{
    pthread_mutex_lock(&r->lock);
    *out = r->counts;
    pthread_mutex_unlock(&r->lock);
}

void resolver_report(Resolver *r, FILE *to)
{
    AssetCounts c;
    asset_counts(r, &c);
    fprintf(to, "assets this session: different assets asked for, by kind and where each came from\n");
    fprintf(to, "(pack: how many are past the distinctness threshold, %.2f, in brackets)\n", r->distinctness);
    fprintf(to, "%-10s %12s %8s %8s %9s %12s %7s\n", "kind", "pack", "server", "overlay", "blizzard", "placeholder", "total");
    uint32_t col[ASSET_SOURCE_COUNT] = { 0 }, distinct = 0, all = 0;
    for (int k = 0; k <= ASSET_KIND_COUNT; ++k) {
        const uint32_t *row = k < ASSET_KIND_COUNT ? c.by[k] : col;
        uint32_t d = k < ASSET_KIND_COUNT ? c.distinct[k] : distinct, total = 0;
        for (int s = 0; s < ASSET_SOURCE_COUNT; ++s) total += row[s];
        char pack[32];
        snprintf(pack, sizeof pack, "%u (%u)", row[ASSET_FROM_PACK], d);
        fprintf(to, "%-10s %12s %8u %8u %9u %12u %7u\n", k < ASSET_KIND_COUNT ? asset_kind_name((AssetKind)k) : "total", pack,
                row[ASSET_FROM_SERVER], row[ASSET_FROM_OVERLAY], row[ASSET_FROM_BLIZZARD], row[ASSET_FROM_PLACEHOLDER], k < ASSET_KIND_COUNT ? total : all);
        if (k < ASSET_KIND_COUNT) {
            for (int s = 0; s < ASSET_SOURCE_COUNT; ++s) col[s] += row[s];
            distinct += d;
            all += total;
        }
    }
    fprintf(to, "placeholders stood in %u time%s; strict mode %s\n", c.placeholder_answers, c.placeholder_answers == 1 ? "" : "s",
            r->strict ? "on (each of those was an error)" : "off");
}

int resolver_exit_status(Resolver *r)
{
    AssetCounts c;
    asset_counts(r, &c);
    return r->strict && c.placeholder_answers > 0 ? 1 : 0;
}
/* }}} */
