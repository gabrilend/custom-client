/*
 * 104-wdb.c (issue 509) - the server's answers, kept on disk between runs
 *
 * See 104-wdb.h. One table holds every key the cache knows of, kept or
 * asked for, by open addressing; the waiters are a list beside it, told
 * in the order they wanted; the queue is the keys asked for that wait for
 * the next window, in the order they were wanted.
 */
#include "104-wdb.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

typedef struct {
    uint32_t       key;
    uint8_t        used, kept, pending;
    unsigned char *rec;
    uint32_t       len;
} Slot;

typedef struct {
    uint32_t key;
    uint64_t waiter;
} Waiter;

struct WdbCache {
    const WdbKind *kind;
    uint32_t       locale, version;
    WdbAsk         ask;
    WdbTell        tell;
    void          *ctx;
    Slot          *slots;
    size_t         nslots, used, kept;
    Waiter        *waiters;
    size_t         nwaiters, capwaiters;
    uint32_t      *queue;
    size_t         qhead, qtail, qcap;
    int            open, sent;          /* the window: begun, and queries sent in it */
    double         start;
};

/* {{{ helpers */
static int on_disk(const WdbKind *k)
{
    return k->file && (k->signature[0] || k->signature[1] || k->signature[2] || k->signature[3]);
}

static void put32(unsigned char *p, uint32_t v)
{
    p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8); p[2] = (unsigned char)(v >> 16); p[3] = (unsigned char)(v >> 24);
}

static uint32_t get32(const unsigned char *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static size_t hash(uint32_t key, size_t n)
{
    return (size_t)((key * 2654435761u) & (uint32_t)(n - 1));
}

/* The slot of `key`, or the empty one where it would go. */
static Slot *slot_of(Slot *slots, size_t n, uint32_t key)
{
    size_t i = hash(key, n);
    while (slots[i].used && slots[i].key != key) i = (i + 1) & (n - 1);
    return &slots[i];
}

/* The slot of `key`, made if missing; NULL when memory runs out. */
static Slot *slot_make(WdbCache *c, uint32_t key)
{
    if ((c->used + 1) * 4 > c->nslots * 3) {
        size_t n = c->nslots * 2;
        Slot *s = calloc(n, sizeof *s);
        if (!s) return NULL;
        for (size_t i = 0; i < c->nslots; ++i)
            if (c->slots[i].used) *slot_of(s, n, c->slots[i].key) = c->slots[i];
        free(c->slots);
        c->slots = s;
        c->nslots = n;
    }
    Slot *s = slot_of(c->slots, c->nslots, key);
    if (!s->used) {
        s->used = 1;
        s->key = key;
        ++c->used;
    }
    return s;
}

static Slot *slot_find(const WdbCache *c, uint32_t key)
{
    Slot *s = slot_of(c->slots, c->nslots, key);
    return s->used ? s : NULL;
}

/* Keeps a copy of the record in `s`. */
static int keep(WdbCache *c, Slot *s, const unsigned char *rec, uint32_t len)
{
    unsigned char *copy = malloc(len ? len : 1);
    if (!copy) return 0;
    if (len) memcpy(copy, rec, len);
    free(s->rec);
    if (!s->kept) ++c->kept;
    s->rec = copy;
    s->len = len;
    s->kept = 1;
    return 1;
}
/* }}} */

/* {{{ wdb_new / wdb_free */
WdbCache *wdb_new(const WdbKind *kind, uint32_t locale, WdbAsk ask, WdbTell tell, void *ctx, WWhy *why)
{
    if (!kind) { w_fail(why, W_ERR_REFUSED, "a cache needs a kind"); return NULL; }
    if (on_disk(kind) && kind->record_size == 0) {
        w_fail(why, W_ERR_REFUSED, "the %s cache is kept on disk but has no record size for its file's header", kind->name);
        return NULL;
    }
    if (kind->limit < 0) { w_fail(why, W_ERR_REFUSED, "the %s cache's query limit %d is below 0", kind->name, kind->limit); return NULL; }
    WdbCache *c = calloc(1, sizeof *c);
    if (c) c->slots = calloc(64, sizeof *c->slots);
    if (!c || !c->slots) { free(c); w_fail(why, W_ERR_FULL, "no memory for the %s cache", kind->name); return NULL; }
    c->nslots = 64;
    c->kind = kind;
    c->locale = locale;
    c->ask = ask;
    c->tell = tell;
    c->ctx = ctx;
    return c;
}

void wdb_free(WdbCache *c)
{
    if (!c) return;
    for (size_t i = 0; i < c->nslots; ++i) free(c->slots[i].rec);
    free(c->slots);
    free(c->waiters);
    free(c->queue);
    free(c);
}
/* }}} */

/* {{{ asking: the window and the queue */
/* Ends the window once 30 seconds have passed since it began. */
static void roll(WdbCache *c, double now)
{
    if (c->open && now - c->start >= WDB_WINDOW_SECONDS) c->open = 0;
}

/* Sends the query for `key` if this window has room: 1 sent, 0 not. */
static int send_now(WdbCache *c, uint32_t key, double now)
{
    if (c->kind->limit) {
        if (!c->open) {
            c->open = 1;
            c->start = now;
            c->sent = 0;
        }
        if (c->sent >= c->kind->limit) return 0;
        ++c->sent;
    }
    if (c->ask) c->ask(c->ctx, c->kind, key);
    return 1;
}

static int enqueue(WdbCache *c, uint32_t key)
{
    if (c->qtail == c->qcap) {
        if (c->qhead) {
            memmove(c->queue, c->queue + c->qhead, (c->qtail - c->qhead) * sizeof *c->queue);
            c->qtail -= c->qhead;
            c->qhead = 0;
        } else {
            size_t n = c->qcap ? c->qcap * 2 : 64;
            uint32_t *q = realloc(c->queue, n * sizeof *q);
            if (!q) return 0;
            c->queue = q;
            c->qcap = n;
        }
    }
    c->queue[c->qtail++] = key;
    return 1;
}

int wdb_tick(WdbCache *c, double now)
{
    int n = 0;
    roll(c, now);
    while (c->qhead < c->qtail && send_now(c, c->queue[c->qhead], now)) {
        ++c->qhead;
        ++n;
    }
    if (c->qhead == c->qtail) c->qhead = c->qtail = 0;
    return n;
}

int wdb_queued(const WdbCache *c)
{
    return (int)(c->qtail - c->qhead);
}
/* }}} */

/* {{{ wdb_find / wdb_want */
int wdb_find(const WdbCache *c, uint32_t key, const unsigned char **rec, uint32_t *len)
{
    Slot *s = slot_find(c, key);
    if (!s || !s->kept) return 0;
    if (rec) *rec = s->rec;
    if (len) *len = s->len;
    return 1;
}

int wdb_want(WdbCache *c, uint32_t key, uint64_t waiter, double now, const unsigned char **rec, uint32_t *len, WWhy *why)
{
    if (wdb_find(c, key, rec, len)) return 1;
    if (c->nwaiters == c->capwaiters) {
        size_t n = c->capwaiters ? c->capwaiters * 2 : 32;
        Waiter *w = realloc(c->waiters, n * sizeof *w);
        if (!w) { w_fail(why, W_ERR_FULL, "no memory for another waiter on the %s cache", c->kind->name); return -1; }
        c->waiters = w;
        c->capwaiters = n;
    }
    Slot *s = slot_make(c, key);
    if (!s) { w_fail(why, W_ERR_FULL, "no memory for another key in the %s cache", c->kind->name); return -1; }
    c->waiters[c->nwaiters++] = (Waiter){ key, waiter };
    if (s->pending) return 0;
    s->pending = 1;
    wdb_tick(c, now);                 /* what waits goes first: if any still waits, the window is full */
    if (!send_now(c, key, now)) {
        if (!enqueue(c, key)) {
            s->pending = 0;
            --c->nwaiters;
            w_fail(why, W_ERR_FULL, "no memory to queue a query on the %s cache", c->kind->name);
            return -1;
        }
    }
    return 0;
}
/* }}} */

/* {{{ wdb_answer */
WErr wdb_answer(WdbCache *c, uint32_t id, const unsigned char *rec, uint32_t len, WWhy *why)
{
    int ok = !(id & WDB_NO_SUCH_RECORD);
    uint32_t key = id & ~WDB_NO_SUCH_RECORD;
    if (ok && len > WDB_RECORD_MAX)
        return w_fail(why, W_ERR_REFUSED, "the %s cache's answer for %u is %u bytes, over %u", c->kind->name, key, len, WDB_RECORD_MAX);
    Slot *s = ok ? slot_make(c, key) : slot_find(c, key);
    if (ok && (!s || !keep(c, s, rec, len)))
        return w_fail(why, W_ERR_FULL, "no memory to keep the %s cache's answer for %u", c->kind->name, key);
    if (s) s->pending = 0;
    /* told in the order they wanted; a waiter's telling may want again, so
     * the told ones are taken out first */
    size_t told = 0, kept = 0, n = c->nwaiters;
    Waiter *mine = n ? malloc(n * sizeof *mine) : NULL;
    for (size_t i = 0; i < n; ++i) {
        if (c->waiters[i].key == key && mine) mine[told++] = c->waiters[i];
        else c->waiters[kept++] = c->waiters[i];
    }
    c->nwaiters = kept;
    for (size_t i = 0; i < told; ++i) {
        const unsigned char *r = NULL;
        uint32_t l = 0;
        if (ok) wdb_find(c, key, &r, &l);
        if (c->tell) c->tell(c->ctx, key, mine[i].waiter, ok, r, l);
    }
    free(mine);
    return W_OK;
}
/* }}} */

/* {{{ versions and counts */
int wdb_server_version(WdbCache *c, uint32_t version)
{
    if (version == c->version) return 0;
    c->version = version;
    for (size_t i = 0; i < c->nslots; ++i) {
        Slot *s = &c->slots[i];
        free(s->rec);
        s->rec = NULL;
        s->len = 0;
        s->kept = 0;
    }
    c->kept = 0;
    return 1;
}

uint32_t wdb_version(const WdbCache *c) { return c->version; }
int      wdb_count(const WdbCache *c) { return (int)c->kept; }
/* }}} */

/* {{{ the file */
static WErr file_path(const WdbCache *c, const char *folder, char *out, size_t n, WWhy *why)
{
    if ((size_t)snprintf(out, n, "%s/%s", folder, c->kind->file) >= n)
        return w_fail(why, W_ERR_REFUSED, "the %s cache's path under %s is too long", c->kind->name, folder);
    return W_OK;
}

static void header(const WdbCache *c, uint32_t h[6])
{
    const WdbKind *k = c->kind;
    h[0] = get32((const unsigned char *)k->signature);
    h[1] = WDB_BUILD;
    h[2] = c->locale;
    h[3] = k->record_size;
    h[4] = k->format_version;
    h[5] = c->version;
}

/* A sentence for the log, with the code left W_OK: the file was ignored, not a failure. */
static WErr ignored(WWhy *why, const char *format, ...) __attribute__((format(printf, 2, 3)));
static WErr ignored(WWhy *why, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vsnprintf(why->text, sizeof why->text, format, ap);
    va_end(ap);
    why->code = W_OK;
    return W_OK;
}

WErr wdb_load(WdbCache *c, const char *folder, int *loaded, WWhy *why)
{
    *loaded = 0;
    if (!on_disk(c->kind)) return W_OK;
    char path[1024];
    if (file_path(c, folder, path, sizeof path, why)) return why->code;
    FILE *f = fopen(path, "rb");
    if (!f) {
        if (errno == ENOENT) return ignored(why, "%s: no %s cache file yet", path, c->kind->name);
        return w_fail(why, W_ERR_IO, "%s: can't open the %s cache: %s", path, c->kind->name, strerror(errno));
    }
    unsigned char *data = NULL;
    size_t len = 0, cap = 0, got;
    for (;;) {
        if (len == cap) {
            size_t n2 = cap ? cap * 2 : 65536;
            unsigned char *d = realloc(data, n2);
            if (!d) { free(data); fclose(f); return w_fail(why, W_ERR_FULL, "%s: no memory to read it", path); }
            data = d;
            cap = n2;
        }
        got = fread(data + len, 1, cap - len, f);
        len += got;
        if (got == 0) break;
    }
    int err = ferror(f);
    fclose(f);
    if (err) { free(data); return w_fail(why, W_ERR_IO, "%s: can't read the %s cache", path, c->kind->name); }

    static const char *const fields[5] = { "signature", "build", "locale", "record size", "format version" };
    uint32_t want[6];
    header(c, want);
    if (len < 24) { free(data); return ignored(why, "%s: ignored: it is shorter than its header (%zu bytes)", path, len); }
    for (int i = 0; i < 5; ++i) {
        uint32_t have = get32(data + 4 * i);
        if (have != want[i]) {
            free(data);
            return ignored(why, "%s: ignored: its %s is %u, this cache's %u", path, fields[i], have, want[i]);
        }
    }
    /* walk it once to be sure it ends, then keep */
    size_t at = 24, entries = 0;
    for (;;) {
        if (len - at < 8) { free(data); return ignored(why, "%s: ignored: it stops mid-entry (after %zu entries)", path, entries); }
        uint32_t key = get32(data + at), n = get32(data + at + 4);
        at += 8;
        if (key == 0 && n == 0) break;
        if (n > WDB_RECORD_MAX || n > len - at) { free(data); return ignored(why, "%s: ignored: it stops mid-entry (after %zu entries)", path, entries); }
        at += n;
        ++entries;
    }
    for (size_t p = 24, i = 0; i < entries; ++i) {
        uint32_t key = get32(data + p), n = get32(data + p + 4);
        Slot *s = slot_make(c, key);
        if (!s || !keep(c, s, data + p + 8, n)) { free(data); return w_fail(why, W_ERR_FULL, "%s: no memory to keep its entries", path); }
        p += 8 + n;
    }
    c->version = get32(data + 20);
    free(data);
    *loaded = (int)entries;
    return W_OK;
}

static int by_key(const void *a, const void *b)
{
    uint32_t x = (*(Slot *const *)a)->key, y = (*(Slot *const *)b)->key;
    return x < y ? -1 : x > y;
}

WErr wdb_save(const WdbCache *c, const char *folder, WWhy *why)
{
    if (!on_disk(c->kind)) return W_OK;
    char path[1024], part[1100];
    if (file_path(c, folder, path, sizeof path, why)) return why->code;
    snprintf(part, sizeof part, "%s.part", path);
    Slot **order = malloc((c->kept ? c->kept : 1) * sizeof *order);
    if (!order) return w_fail(why, W_ERR_FULL, "no memory to write the %s cache", c->kind->name);
    size_t n = 0;
    for (size_t i = 0; i < c->nslots; ++i)
        if (c->slots[i].used && c->slots[i].kept) order[n++] = &c->slots[i];
    qsort(order, n, sizeof *order, by_key);            /* key order: the same cache writes the same bytes */
    FILE *f = fopen(part, "wb");
    if (!f) { free(order); return w_fail(why, W_ERR_IO, "%s: can't write the %s cache: %s", part, c->kind->name, strerror(errno)); }
    uint32_t h[6];
    unsigned char b[24];
    header(c, h);
    for (int i = 0; i < 6; ++i) put32(b + 4 * i, h[i]);
    int bad = fwrite(b, 1, 24, f) != 24;
    for (size_t i = 0; i < n && !bad; ++i) {
        put32(b, order[i]->key);
        put32(b + 4, order[i]->len);
        bad = fwrite(b, 1, 8, f) != 8 || (order[i]->len && fwrite(order[i]->rec, 1, order[i]->len, f) != order[i]->len);
    }
    memset(b, 0, 8);
    bad = bad || fwrite(b, 1, 8, f) != 8;
    free(order);
    if (fclose(f) != 0) bad = 1;
    if (bad || rename(part, path) != 0) {
        remove(part);
        return w_fail(why, W_ERR_IO, "%s: can't write the %s cache", path, c->kind->name);
    }
    return W_OK;
}

WErr wdb_close(WdbCache *c, const char *folder, WWhy *why)
{
    WErr e = wdb_save(c, folder, why);
    wdb_free(c);
    return e;
}
/* }}} */

/* {{{ wdb_folder */
WErr wdb_folder(const char *root, int data_present, const char *locale, char *out, size_t n, WWhy *why)
{
    if (data_present && (!locale || !*locale || strchr(locale, '/') || !strcmp(locale, ".") || !strcmp(locale, "..")))
        return w_fail(why, W_ERR_REFUSED, "the locale \"%s\" is not a folder name", locale ? locale : "");
    int w = data_present ? snprintf(out, n, "%s/Cache/WDB/%s", root, locale) : snprintf(out, n, "%s/WDB", root);
    if (w < 0 || (size_t)w >= n) return w_fail(why, W_ERR_REFUSED, "the WDB folder's path under %s is too long", root);
    for (char *p = out + strlen(root); ; ++p) {          /* make root, each folder below it, then the last */
        if (*p == '/' || *p == 0) {
            char was = *p;
            *p = 0;
            int r = mkdir(out, 0755);
            *p = was;
            if (r != 0 && errno != EEXIST) return w_fail(why, W_ERR_IO, "can't make the folder %s: %s", out, strerror(errno));
            if (!was) break;
        }
    }
    return W_OK;
}
/* }}} */
