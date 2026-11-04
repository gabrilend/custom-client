/*
 * 074-tables.c (issue 106) - see 074-tables.h
 *
 * For a general audience: the first time a table is asked for, every
 * copy is read and one list made of each row number and the copy that
 * answers it (the first copy holding it); then each lookup is a search
 * in that list. One lock covers the tables, since the reading-ahead
 * thread may ask too.
 */
#include "074-tables.h"
#include "013-errors.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

/* {{{ the tables' state */
typedef struct { uint32_t id, row; int copy; } TableIndex;

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
} Table;

struct Tables {
    Resolver       *assets;
    MpqChain       *chain;
    pthread_mutex_t lock;
    Table         **tables;  int ntables, cap;
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
WErr tables_row(Tables *ts, const char *name, uint32_t id, TableRow *out, WWhy *why)
{
    memset(out, 0, sizeof *out);
    if (!name || !name[0] || strlen(name) >= 64) return w_fail(why, W_ERR_REFUSED, "a table's name is 1 to 63 characters (\"%.80s\")", name ? name : "");
    pthread_mutex_lock(&ts->lock);
    Table *t = table(ts, name);
    WErr rc = W_OK;
    if (t->state < 0) rc = w_fail(why, t->fail.code, "%s", t->fail.text);
    else {
        TableIndex probe = { id, 0, 0 }, *hit = NULL;
        size_t lo = 0, hi = t->nindex;                   /* the number's entry: there is one at most */
        while (lo < hi) {
            size_t mid = (lo + hi) / 2;
            if (t->index[mid].id < probe.id) lo = mid + 1; else hi = mid;
        }
        if (lo < t->nindex && t->index[lo].id == id) hit = &t->index[lo];
        if (!hit) rc = w_fail(why, W_ERR_NOT_FOUND, "no row %u in %s (%d cop%s: %u row%s)", id, name, t->ncopies, t->ncopies == 1 ? "y" : "ies",
                              t->nindex, t->nindex == 1 ? "" : "s");
        else {
            ++t->answered[hit->copy];
            out->dbc = &t->dbc[hit->copy];
            out->row = hit->row;
            out->source = t->copy[hit->copy].source;
            out->from = t->from[hit->copy];
        }
    }
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
        fprintf(to, "\n");
    }
    pthread_mutex_unlock(&ts->lock);
}
/* }}} */
