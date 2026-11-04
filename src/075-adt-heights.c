/*
 * 075-adt-heights.c (issue 1109) - see 075-adt-heights.h
 *
 * For a general audience: each chunk's 145 heights copied into their
 * places in the tile's two grids; the tile files read through the
 * resolver, the map's WDT once at the start.
 */
#include "075-adt-heights.h"
#include "013-errors.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* {{{ adt_heights_from_tile */
WErr adt_heights_from_tile(const AdtTile *adt, int tx, int ty, MapTile **out, WWhy *why)
{
    *out = NULL;
    for (int k = 0; k < 256; ++k)
        if (adt->chunks[k].ix != k % 16 || adt->chunks[k].iy != k / 16)
            return w_fail(why, W_ERR_CORRUPT, "tile (%d, %d): chunk %d says it is column %d, row %d; its place is column %d, row %d",
                          tx, ty, k, adt->chunks[k].ix, adt->chunks[k].iy, k % 16, k / 16);
    MapTile *t = calloc(1, sizeof *t);
    t->tx = tx; t->ty = ty;
    t->v9 = malloc(sizeof(float) * 129 * 129);
    t->v8 = malloc(sizeof(float) * 128 * 128);
    for (int k = 0; k < 256; ++k) {
        const AdtChunk *c = &adt->chunks[k];
        int cy = k / 16, cx = k % 16;
        for (int r = 0; r <= 8; ++r)
            for (int col = 0; col <= 8; ++col)
                t->v9[(cy * 8 + r) * 129 + cx * 8 + col] = c->z + c->heights[adt_corner(r, col)];
        for (int r = 0; r < 8; ++r)
            for (int col = 0; col < 8; ++col)
                t->v8[(cy * 8 + r) * 128 + cx * 8 + col] = c->z + c->heights[adt_centre(r, col)];
        /* the same 4 x 4 bits a chunk the server's holes keep (018-maps.c's maps_is_hole) */
        t->holes[cy * 16 + cx] = c->holes;
        if (c->holes) t->has_holes = 1;
    }
    *out = t;
    return W_OK;
}
/* }}} */

/* {{{ the source */
struct AdtHeights {
    Resolver *assets;
    MpqChain *chain;
    char      dir[64];
    int       big_alpha;
    int       made;                   /* counted with atomic adds: either thread asks */
};

WErr adt_heights_open(Resolver *assets, MpqChain *chain, const char *map_dir, AdtHeights **out, WWhy *why)
{
    *out = NULL;
    if (!map_dir[0] || strlen(map_dir) >= 64) return w_fail(why, W_ERR_REFUSED, "a map's folder name is 1 to 63 characters (\"%.80s\")", map_dir);
    char path[200];
    snprintf(path, sizeof path, "World\\Maps\\%s\\%s.wdt", map_dir, map_dir);
    AssetAnswer a;
    WErr rc = asset_read(assets, chain, path, ASSET_GROUND, &a, why);
    if (rc) return rc == W_ERR_NOT_FOUND ? w_fail(why, rc, "the map's %s is in none of the places asked, so it has no ground to stand on", path) : rc;
    WWhy inner;
    int big = adt_wdt_big_alpha(a.data, a.len, &inner);
    asset_answer_free(&a);
    if (big < 0) return w_fail(why, inner.code, "%s: %s", path, inner.text);
    AdtHeights *h = calloc(1, sizeof *h);
    h->assets = assets;
    h->chain = chain;
    snprintf(h->dir, sizeof h->dir, "%s", map_dir);
    h->big_alpha = big;
    *out = h;
    return W_OK;
}

void adt_heights_free(AdtHeights *h) { free(h); }

int adt_heights_made(const AdtHeights *h) { return __atomic_load_n(&h->made, __ATOMIC_RELAXED); }

MapTile *adt_heights_tile(void *ctx, int map_id, int tx, int ty, WWhy *why)
{
    AdtHeights *h = ctx;
    (void)map_id;                                        /* the files go by the map's folder, not its number */
    if (tx < 0 || tx > 63 || ty < 0 || ty > 63) { w_fail(why, W_ERR_NOT_FOUND, "tile (%d, %d) is off the map's 64 x 64 grid", tx, ty); return NULL; }
    char path[200];
    snprintf(path, sizeof path, "World\\Maps\\%s\\%s_%d_%d.adt", h->dir, h->dir, ty, tx);   /* west-east number first */
    AssetAnswer a;
    WErr rc = asset_read(h->assets, h->chain, path, ASSET_GROUND, &a, why);
    if (rc) return NULL;                                 /* W_ERR_NOT_FOUND: open sea */
    AdtTile *adt = malloc(sizeof *adt);
    WWhy inner;
    rc = adt_read(a.data, a.len, h->big_alpha, adt, &inner);
    if (rc) {
        w_fail(why, rc, "%s (from %s): %s", path, a.from ? a.from : "?", inner.text);
        asset_answer_free(&a);
        free(adt);
        return NULL;
    }
    asset_answer_free(&a);
    MapTile *t;
    rc = adt_heights_from_tile(adt, tx, ty, &t, &inner);
    adt_free(adt);
    free(adt);
    if (rc) { w_fail(why, rc, "%s: %s", path, inner.text); return NULL; }
    __atomic_add_fetch(&h->made, 1, __ATOMIC_RELAXED);
    return t;
}
/* }}} */
