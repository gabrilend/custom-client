/*
 * 077-slk.c (issue 302) - Warcraft III's spreadsheets (SYLK); see 077-slk.h
 *
 * For a general audience: every cell is kept as (column, row, text),
 * sorted by its place with only the last one written for a place kept,
 * as the Lua's table keeps it; a value is found by halving.
 */
#include "077-slk.h"

#include <stdlib.h>
#include <string.h>

/* {{{ the handle */
typedef struct { int x, y, order; char *value; } SlkCell;
struct Slk {
    SlkCell *cells;  int ncells, cap;
    int     *ids;    int nids;          /* the first cell naming each row's id, by first meeting */
};
/* }}} */

/* {{{ static number: a field's whole number, 1-65535, or -1 */
static int number(const char *p, const char *end)
{
    if (p >= end) return -1;
    long v = 0;
    for (; p < end; ++p) {
        if (*p < '0' || *p > '9') return -1;
        v = v * 10 + (*p - '0');
        if (v > 65535) return -1;
    }
    return v >= 1 ? (int)v : -1;
}
/* }}} */

/* {{{ static decode: a K field's value as text (";;" one ";", quotes off) */
static char *decode(const char *p, const char *end)
{
    char *out = malloc((size_t)(end - p) + 1), *o = out;
    if (p < end && *p == '"') {
        ++p;
        if (end > p && end[-1] == '"') --end;
        while (p < end) {
            if (p[0] == ';' && p + 1 < end && p[1] == ';') { *o++ = ';'; p += 2; }
            else *o++ = *p++;
        }
    } else while (p < end) *o++ = *p++;
    *o = '\0';
    return out;
}
/* }}} */

/* {{{ slk_read */
void slk_free(Slk *k)
{
    if (!k) return;
    for (int i = 0; i < k->ncells; ++i) free(k->cells[i].value);
    free(k->cells);
    free(k->ids);
    free(k);
}

/* cells in place order (row, then column), only the last written for each place kept */
static int by_place(const void *a, const void *b)
{
    const SlkCell *p = a, *q = b;
    if (p->y != q->y) return p->y < q->y ? -1 : 1;
    if (p->x != q->x) return p->x < q->x ? -1 : 1;
    return p->order < q->order ? -1 : p->order > q->order;
}
static const char *cell_at(const Slk *k, int x, int y)
{
    int lo = 0, hi = k->ncells - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        const SlkCell *c = &k->cells[mid];
        if (c->y == y && c->x == x) return c->value;
        if (c->y < y || (c->y == y && c->x < x)) lo = mid + 1; else hi = mid - 1;
    }
    return NULL;
}

WErr slk_read(const unsigned char *s, size_t len, Slk **out, WWhy *why)
{
    *out = NULL;
    Slk *k = calloc(1, sizeof *k);
    const char *p = (const char *)s, *end = p + len;
    int head = 0, x = 1, y = 1, line = 0;
    while (p < end) {
        const char *eol = memchr(p, '\n', (size_t)(end - p));
        const char *stop = eol ? eol : end;
        const char *next = eol ? eol + 1 : end;
        if (stop > p && stop[-1] == '\r') --stop;
        ++line;
        if (stop - p >= 3 && !memcmp(p, "ID;", 3)) head = 1;
        else if (stop - p == 1 && *p == 'E') break;
        else if (stop - p >= 2 && p[0] == 'C' && p[1] == ';') {
            const char *f = p + 2;
            const char *value = NULL;
            while (f < stop) {
                if (*f == 'K') { value = f + 1; break; }
                const char *semi = memchr(f, ';', (size_t)(stop - f));
                const char *fend = semi ? semi : stop;
                if (*f == 'X' || *f == 'Y') {
                    int v = number(f + 1, fend);
                    if (v < 0) { slk_free(k); return w_fail(why, W_ERR_CORRUPT, "line %d: a cell's %c is %.*s, not a whole number from 1 to 65535", line, *f, (int)(fend - f - 1), f + 1); }
                    if (*f == 'X') x = v; else y = v;
                }
                f = semi ? semi + 1 : stop;
            }
            if (value) {
                if (k->ncells == k->cap) { k->cap = k->cap ? k->cap * 2 : 256; k->cells = realloc(k->cells, sizeof *k->cells * (size_t)k->cap); }
                k->cells[k->ncells] = (SlkCell){ x, y, k->ncells, decode(value, stop) };
                ++k->ncells;
            }
        }
        p = next;
    }
    if (!head) { slk_free(k); return w_fail(why, W_ERR_CORRUPT, "not a SYLK sheet (no \"ID;\" head line)"); }
    /* each row's name, once, in the order first met (rows from 2), by file order */
    k->ids = malloc(sizeof *k->ids * (size_t)(k->ncells ? k->ncells : 1));
    char **first = malloc(sizeof *first * (size_t)(k->ncells ? k->ncells : 1));
    int nfirst = 0;
    for (int i = 0; i < k->ncells; ++i) {
        if (k->cells[i].x != 1 || k->cells[i].y < 2) continue;
        int seen = 0;
        for (int j = 0; j < nfirst && !seen; ++j) seen = !strcmp(first[j], k->cells[i].value);
        if (!seen) first[nfirst++] = k->cells[i].value;
    }
    /* sorted by place, the last written for each place kept */
    qsort(k->cells, (size_t)k->ncells, sizeof *k->cells, by_place);
    int n = 0;
    for (int i = 0; i < k->ncells; ++i) {
        if (n && k->cells[n - 1].x == k->cells[i].x && k->cells[n - 1].y == k->cells[i].y) { free(k->cells[n - 1].value); k->cells[n - 1] = k->cells[i]; }
        else k->cells[n++] = k->cells[i];
    }
    k->ncells = n;
    if (!cell_at(k, 1, 1)) { free(first); slk_free(k); return w_fail(why, W_ERR_CORRUPT, "the sheet's row 1 names no column 1 (its rows' names)"); }
    /* the names first met, as they read now (a name a later cell replaced is that row's newer name) */
    for (int j = 0; j < nfirst; ++j)
        for (int i = 0; i < k->ncells; ++i)
            if (k->cells[i].x == 1 && k->cells[i].y >= 2 && !strcmp(k->cells[i].value, first[j])) { k->ids[k->nids++] = i; break; }
    free(first);
    *out = k;
    return W_OK;
}
/* }}} */

/* {{{ slk_value, slk_rows, slk_row_id
 * As the Lua: a column's name is row 1's (the last column of that name
 * wins), and of the rows named `id` the last one holding that column's
 * cell answers. */
const char *slk_value(const Slk *k, const char *id, const char *column)
{
    int x = -1;
    for (int i = 0; i < k->ncells && k->cells[i].y == 1; ++i)
        if (!strcmp(k->cells[i].value, column)) x = k->cells[i].x;
    if (x < 0) return NULL;
    const char *got = NULL;
    for (int i = 0; i < k->ncells; ++i) {
        const SlkCell *c = &k->cells[i];
        if (c->x != 1 || c->y < 2 || strcmp(c->value, id)) continue;
        const char *v = cell_at(k, x, c->y);
        if (v) got = v;
    }
    return got;
}
int slk_rows(const Slk *k) { return k->nids; }
const char *slk_row_id(const Slk *k, int i) { return i >= 0 && i < k->nids ? k->cells[k->ids[i]].value : NULL; }
/* }}} */
