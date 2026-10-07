/*
 * 035-creatures.c (issue 173t) - where the world's creatures stand; see 035-creatures.h
 *
 * For a general audience: reads the generated list line by line, notes
 * each creature's map tile, and sorts the list by tile so a tile's
 * creatures sit together and are found by halving the list.
 */
#include "035-creatures.h"
#include "013-errors.h"
#include "018-maps.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* {{{ static by_tile: the sort's order (tile, then guid, so it's stable) */
static int by_tile(const void *a, const void *b)
{
    const Spawn *x = a, *y = b;
    if (x->tile != y->tile) return x->tile < y->tile ? -1 : 1;
    return x->guid < y->guid ? -1 : x->guid > y->guid;
}
/* }}} */

/* {{{ creatures_read */
WErr creatures_read(const char *path, SpawnList *out, WWhy *why)
{
    memset(out, 0, sizeof *out);
    FILE *f = fopen(path, "r");
    if (!f) {
        w_fail(why, W_ERR_NOT_FOUND, "%.200s: %s (make it with wow-chat-2026's scripts/generate-custom-client-creatures; running without creatures)", path, strerror(errno));
        return why->code;
    }
    char line[512];
    int cap = 0, number = 0;
    while (fgets(line, sizeof line, f)) {
        ++number;
        if (line[0] == '#' || line[0] == '\n') continue;
        Spawn s;
        memset(&s, 0, sizeof s);
        int name_at = 0;
        /* the name is the rest of the line after the eighth tab */
        if (sscanf(line, "%u\t%u\t%d\t%f\t%lf\t%lf\t%lf\t%f\t%n", &s.guid, &s.entry, &s.display, &s.size, &s.x, &s.y, &s.z, &s.facing, &name_at) != 8 || !name_at) {
            w_fail(why, W_ERR_CORRUPT, "%.200s, line %d: not nine tab-separated fields (guid, entry, display, size, x, y, z, facing, name)", path, number);
            fclose(f);
            creatures_free(out);
            return why->code;
        }
        snprintf(s.name, sizeof s.name, "%.*s", (int)strcspn(line + name_at, "\r\n"), line + name_at);
        int tx, ty;
        maps_tile_of((float)s.x, (float)s.y, &tx, &ty);
        s.tile = tx * 64 + ty;
        if (out->n == cap) { cap = cap ? cap * 2 : 1024; out->spawns = realloc(out->spawns, sizeof *out->spawns * cap); }
        out->spawns[out->n++] = s;
    }
    fclose(f);
    qsort(out->spawns, (size_t)out->n, sizeof *out->spawns, by_tile);
    return 0;
}
/* }}} */

/* {{{ creatures_in_tile: the first spawn at or after the tile, by halving */
int creatures_in_tile(const SpawnList *l, int tx, int ty, int *first)
{
    int tile = tx * 64 + ty, lo = 0, hi = l->n;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (l->spawns[mid].tile < tile) lo = mid + 1; else hi = mid;
    }
    int end = lo;
    while (end < l->n && l->spawns[end].tile == tile) ++end;
    *first = lo;
    return end - lo;
}
/* }}} */

void creatures_free(SpawnList *l) { free(l->spawns); memset(l, 0, sizeof *l); }
