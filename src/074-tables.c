/*
 * 074-tables.c (issue 106) - see 074-tables.h
 *
 * For a general audience: the first time a table is asked for, every
 * copy is read and one list made of each row number and the copy that
 * answers it (the first copy holding it); then each lookup is a search
 * in that list. A search by something else walks a second list, made
 * when first asked; a placeholder row is made once and kept. One lock
 * covers the tables, since the reading-ahead thread may ask too.
 */
#include "074-tables.h"
#include "013-errors.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* {{{ the tables' state */
typedef struct { uint32_t id, row; int copy; } TableIndex;
typedef struct { uint32_t id; unsigned char *bytes; Dbc dbc; } Placeholder;   /* a row made here (tables_row_or_placeholder) */

typedef struct {
    char        name[64];
    int         state;                        /* 1 read, -1 failed (with `fail`) */
    WWhy        fail;
    int         ncopies;
    AssetAnswer copy[TABLES_COPIES_MAX];      /* each copy's bytes and place */
    char        from[TABLES_COPIES_MAX][600]; /* each copy's folder or archive, kept here */
    Dbc         dbc[TABLES_COPIES_MAX];
    TableIndex *index;  uint32_t nindex;      /* by id: the copy and row answering it */
    uint32_t    answered[TABLES_COPIES_MAX];  /* lookups each copy answered */
    TableAt    *scan;   uint32_t nscan;       /* the rows for a search, made when first asked (tables_scan) */
    int         scanned;
    Placeholder **made;  int nmade, made_cap; /* placeholder rows, each made once */
    uint32_t    refused;                      /* placeholder rows strict mode refused */
} Table;

struct Tables {
    Resolver       *assets;
    MpqChain       *chain;
    pthread_mutex_t lock;
    Table         **tables;  int ntables, cap;
    int             placeholders;            /* placeholder rows made, all tables */
};
/* }}} */

/* {{{ tables_new, tables_free */
Tables *tables_new(Resolver *assets, MpqChain *chain)
{
    Tables *ts = calloc(1, sizeof *ts);
    ts->assets = assets;
    ts->chain = chain;
    pthread_mutex_init(&ts->lock, NULL);
    return ts;
}

void tables_free(Tables *ts)
{
    if (!ts) return;
    for (int i = 0; i < ts->ntables; ++i) {
        Table *t = ts->tables[i];
        for (int c = 0; c < t->ncopies; ++c) asset_answer_free(&t->copy[c]);
        free(t->index);
        free(t->scan);
        for (int k = 0; k < t->nmade; ++k) { free(t->made[k]->bytes); free(t->made[k]); }
        free(t->made);
        free(t);
    }
    free(ts->tables);
    pthread_mutex_destroy(&ts->lock);
    free(ts);
}
/* }}} */

/* {{{ static load: every copy of table `t`, and its index (the lock held) */
static int index_order(const void *a, const void *b)
{
    const TableIndex *x = a, *y = b;
    if (x->id != y->id) return x->id < y->id ? -1 : 1;
    if (x->copy != y->copy) return x->copy < y->copy ? -1 : 1;
    return x->row < y->row ? -1 : x->row > y->row;
}

static WErr load(Tables *ts, Table *t, WWhy *why)
{
    char path[100];
    snprintf(path, sizeof path, "DBFilesClient\\%s.dbc", t->name);
    WErr rc;
    if (ts->assets) rc = asset_resolve_all(ts->assets, path, ASSET_TABLE, t->copy, TABLES_COPIES_MAX, &t->ncopies, why);
    else {
        rc = asset_read(NULL, ts->chain, path, ASSET_TABLE, &t->copy[0], why);
        t->ncopies = rc ? 0 : 1;
        if (rc == W_ERR_NOT_FOUND) rc = W_OK;            /* said below, as with the resolver */
    }
    if (rc) return rc;
    if (!t->ncopies) return w_fail(why, W_ERR_NOT_FOUND, "%s is in none of the places asked (%s)", path, ts->assets ? "packs, server folders, overlays, the archives" : "the client's archives");
    size_t total = 0;
    for (int c = 0; c < t->ncopies; ++c) {
        snprintf(t->from[c], sizeof t->from[c], "%s", t->copy[c].from ? t->copy[c].from : "");
        WWhy inner;
        /* the baseline set's copy, last, of another width than the first:
         * left out, said (a fallback never stops what the places before it
         * answer; 1109 step 1l) */
        if (c && t->copy[c].source == ASSET_FROM_BASELINE && !dbc_open(&t->dbc[c], t->copy[c].data, t->copy[c].len, &inner)
            && t->dbc[c].fields != t->dbc[0].fields) {
            fprintf(stderr, "tables: %s from the baseline set (%s) has %u fields a row, and from %s (%s) %u: the set's rows are left out\n",
                    path, t->from[c], t->dbc[c].fields, asset_source_name(t->copy[0].source), t->from[0], t->dbc[0].fields);
            asset_answer_free(&t->copy[c]);
            for (int k = c + 1; k < t->ncopies; ++k) t->copy[k - 1] = t->copy[k];
            --t->ncopies;
            --c;
            continue;
        }
        if (dbc_open(&t->dbc[c], t->copy[c].data, t->copy[c].len, &inner))
            return w_fail(why, inner.code, "%s from %s (%s): %s", path, asset_source_name(t->copy[c].source), t->from[c], inner.text);
        if (t->dbc[c].fields < 1)
            return w_fail(why, W_ERR_CORRUPT, "%s from %s (%s) has no fields, so no row numbers", path, asset_source_name(t->copy[c].source), t->from[c]);
        if (c && t->dbc[c].fields != t->dbc[0].fields)
            return w_fail(why, W_ERR_REFUSED, "%s from %s (%s) has %u fields a row, and from %s (%s) %u: one was written for another client build, so its rows can't be read as this one's",
                          path, asset_source_name(t->copy[0].source), t->from[0], t->dbc[0].fields,
                          asset_source_name(t->copy[c].source), t->from[c], t->dbc[c].fields);
        total += t->dbc[c].rows;
    }
    t->index = malloc(sizeof *t->index * (total ? total : 1));
    uint32_t n = 0;
    for (int c = 0; c < t->ncopies; ++c)
        for (uint32_t r = 0; r < t->dbc[c].rows; ++r) t->index[n++] = (TableIndex){ dbc_u32(&t->dbc[c], r, 1), r, c };
    qsort(t->index, n, sizeof *t->index, index_order);
    /* the first copy holding a number answers it (and in one copy, its first row with it, as each reader's own loop found) */
    uint32_t kept = 0;
    for (uint32_t i = 0; i < n; ++i)
        if (!kept || t->index[kept - 1].id != t->index[i].id) t->index[kept++] = t->index[i];
    t->nindex = kept;
    return W_OK;
}
/* }}} */

/* {{{ static table: the named table, read when first asked (the lock held) */
static Table *table(Tables *ts, const char *name)
{
    for (int i = 0; i < ts->ntables; ++i) if (!strcmp(ts->tables[i]->name, name)) return ts->tables[i];
    Table *t = calloc(1, sizeof *t);
    snprintf(t->name, sizeof t->name, "%s", name);
    WErr rc = load(ts, t, &t->fail);
    t->state = rc ? -1 : 1;
    if (rc) {                                            /* kept failed: its bytes let go, its sentence kept */
        for (int c = 0; c < t->ncopies; ++c) asset_answer_free(&t->copy[c]);
        t->ncopies = 0;
        free(t->index); t->index = NULL; t->nindex = 0;
    }
    if (ts->ntables == ts->cap) { ts->cap = ts->cap ? ts->cap * 2 : 16; ts->tables = realloc(ts->tables, sizeof *ts->tables * (size_t)ts->cap); }
    ts->tables[ts->ntables++] = t;
    return t;
}
/* }}} */

/* {{{ tables_row, tables_copies, tables_report */
/* the number's entry in the index (one at most), or NULL */
static const TableIndex *find(const Table *t, uint32_t id)
{
    size_t lo = 0, hi = t->nindex;
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (t->index[mid].id < id) lo = mid + 1; else hi = mid;
    }
    return lo < t->nindex && t->index[lo].id == id ? &t->index[lo] : NULL;
}

/* the row, or a placeholder when `stand_in` (the lock held) */
static WErr row_of(Tables *ts, const char *name, uint32_t id, int stand_in, TableRow *out, WWhy *why)
{
    Table *t = table(ts, name);
    if (t->state < 0) return w_fail(why, t->fail.code, "%s", t->fail.text);
    const TableIndex *hit = find(t, id);
    if (hit) {
        ++t->answered[hit->copy];
        out->dbc = &t->dbc[hit->copy];
        out->row = hit->row;
        out->source = t->copy[hit->copy].source;
        out->from = t->from[hit->copy];
        return W_OK;
    }
    WErr rc = w_fail(why, W_ERR_NOT_FOUND, "no row %u in %s (%d cop%s: %u row%s)", id, name, t->ncopies, t->ncopies == 1 ? "y" : "ies",
                     t->nindex, t->nindex == 1 ? "" : "s");
    if (!stand_in || !ts->assets) return rc;
    Placeholder *p = NULL;
    for (int k = 0; k < t->nmade && !p; ++k) if (t->made[k]->id == id) p = t->made[k];
    if (!p) {
        /* said and counted by the resolver, refused in strict mode (each time asked, as its other placeholders) */
        char path[100];
        snprintf(path, sizeof path, "DBFilesClient\\%s.dbc", name);
        WWhy inner;
        if (asset_row_stand_in(ts->assets, path, id, &inner)) { ++t->refused; return w_fail(why, inner.code, "%s", inner.text); }
        uint32_t fields = t->dbc[0].fields, row_len = fields * 4;
        size_t len = 20 + (size_t)row_len + 1;           /* the header, the row, a text block of one empty string */
        p = calloc(1, sizeof *p);
        unsigned char *b = p ? calloc(1, len) : NULL;
        if (t->nmade == t->made_cap) {
            int cap = t->made_cap ? t->made_cap * 2 : 8;
            Placeholder **m = b ? realloc(t->made, sizeof *m * (size_t)cap) : NULL;
            if (m) { t->made = m; t->made_cap = cap; }
        }
        if (!p || !b || t->nmade == t->made_cap) { free(b); free(p); return w_fail(why, W_ERR_FULL, "no memory for a placeholder row %u of %s", id, name); }
        memcpy(b, "WDBC", 4);
        uint32_t head[4] = { 1, fields, row_len, 1 };
        for (int k = 0; k < 4; ++k) for (int q = 0; q < 4; ++q) b[4 + 4 * k + q] = (unsigned char)(head[k] >> (8 * q));
        for (int q = 0; q < 4; ++q) b[20 + q] = (unsigned char)(id >> (8 * q));
        p->id = id;
        p->bytes = b;
        WWhy inner2;
        if (dbc_open(&p->dbc, b, len, &inner2)) { free(b); free(p); return w_fail(why, inner2.code, "a placeholder row of %s: %s", name, inner2.text); }
        t->made[t->nmade++] = p;
        ++ts->placeholders;
    }
    out->dbc = &p->dbc;
    out->row = 0;
    out->source = ASSET_FROM_PLACEHOLDER;
    out->from = "built in";
    out->placeholder = 1;
    return W_OK;
}

WErr tables_row(Tables *ts, const char *name, uint32_t id, TableRow *out, WWhy *why)
{
    memset(out, 0, sizeof *out);
    if (!name || !name[0] || strlen(name) >= 64) return w_fail(why, W_ERR_REFUSED, "a table's name is 1 to 63 characters (\"%.80s\")", name ? name : "");
    pthread_mutex_lock(&ts->lock);
    WErr rc = row_of(ts, name, id, 0, out, why);
    pthread_mutex_unlock(&ts->lock);
    return rc;
}

WErr tables_row_or_placeholder(Tables *ts, const char *name, uint32_t id, TableRow *out, WWhy *why)
{
    memset(out, 0, sizeof *out);
    if (!name || !name[0] || strlen(name) >= 64) return w_fail(why, W_ERR_REFUSED, "a table's name is 1 to 63 characters (\"%.80s\")", name ? name : "");
    pthread_mutex_lock(&ts->lock);
    WErr rc = row_of(ts, name, id, 1, out, why);
    pthread_mutex_unlock(&ts->lock);
    return rc;
}

int tables_placeholders(Tables *ts)
{
    pthread_mutex_lock(&ts->lock);
    int n = ts->placeholders;
    pthread_mutex_unlock(&ts->lock);
    return n;
}

WErr tables_scan(Tables *ts, const char *name, TableScan *out, WWhy *why)
{
    memset(out, 0, sizeof *out);
    if (!name || !name[0] || strlen(name) >= 64) return w_fail(why, W_ERR_REFUSED, "a table's name is 1 to 63 characters (\"%.80s\")", name ? name : "");
    pthread_mutex_lock(&ts->lock);
    Table *t = table(ts, name);
    WErr rc = W_OK;
    if (t->state < 0) rc = w_fail(why, t->fail.code, "%s", t->fail.text);
    else if (!t->scanned) {
        /* the first copy whole, in its order; then each later copy's rows
         * whose number the index gives to it (no earlier copy has it) */
        size_t total = 0;
        for (int c = 0; c < t->ncopies; ++c) total += t->dbc[c].rows;
        t->scan = malloc(sizeof *t->scan * (total ? total : 1));
        if (!t->scan) rc = w_fail(why, W_ERR_FULL, "no memory for %s's %zu rows", name, total);
        else {
            uint32_t n = 0;
            for (int c = 0; c < t->ncopies; ++c)
                for (uint32_t r = 0; r < t->dbc[c].rows; ++r) {
                    if (c) {
                        const TableIndex *hit = find(t, dbc_u32(&t->dbc[c], r, 1));
                        if (!hit || hit->copy != c) continue;
                    }
                    t->scan[n++] = (TableAt){ &t->dbc[c], r, c };
                }
            t->nscan = n;
            t->scanned = 1;
        }
    }
    if (!rc) { out->at = t->scan; out->n = t->nscan; out->ncopies = t->ncopies; }
    pthread_mutex_unlock(&ts->lock);
    return rc;
}

int tables_copies(Tables *ts, const char *name, uint32_t rows[TABLES_COPIES_MAX], AssetSource places[TABLES_COPIES_MAX])
{
    if (!name || !name[0] || strlen(name) >= 64) return -1;
    pthread_mutex_lock(&ts->lock);
    Table *t = table(ts, name);
    int n = t->state < 0 ? -1 : t->ncopies;
    for (int c = 0; c < n; ++c) {
        if (rows) rows[c] = t->dbc[c].rows;
        if (places) places[c] = t->copy[c].source;
    }
    pthread_mutex_unlock(&ts->lock);
    return n;
}

void tables_report(Tables *ts, FILE *to)
{
    pthread_mutex_lock(&ts->lock);
    fprintf(to, "tables this session: each table's copies, first place first (rows; rows asked for that it answered)\n");
    for (int i = 0; i < ts->ntables; ++i) {
        const Table *t = ts->tables[i];
        fprintf(to, "  %-24s", t->name);
        if (t->state < 0) fprintf(to, " not read: %s", t->fail.text);
        for (int c = 0; c < t->ncopies; ++c)
            fprintf(to, "%s %s %s (%u; %u)", c ? "," : "", asset_source_name(t->copy[c].source), t->from[c], t->dbc[c].rows, t->answered[c]);
        if (t->nmade || t->refused) fprintf(to, "; %d placeholder row%s", t->nmade, t->nmade == 1 ? "" : "s");
        if (t->refused) fprintf(to, " (%u refused by strict mode)", t->refused);
        fprintf(to, "\n");
    }
    pthread_mutex_unlock(&ts->lock);
}
/* }}} */
