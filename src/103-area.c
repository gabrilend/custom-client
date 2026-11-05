/*
 * 103-area.c (issue 1109 step 1aa) - the zone she stands in; see 103-area.h
 *
 * For a general audience: finds the ground chunk under her, the area
 * number it carries, and that area's name in AreaTable.
 */
#include "103-area.h"
#include "018-maps.h"
#include "097-table-layouts.h"

#include <stdio.h>
#include <string.h>

/* {{{ static column: a column of AreaTable by name, from the layout (097) */
static uint32_t column(const char *name)
{
    return table_layout_find(table_layout("AreaTable"), name);
}
/* }}} */

/* {{{ area_find */
WErr area_find(Tables *tables, uint32_t id, AreaRow *out, WWhy *why)
{
    memset(out, 0, sizeof *out);
    uint32_t c_continent = column("ContinentID"), c_parent = column("ParentAreaID"), c_name = column("AreaName_enUS");
    if (!c_continent || !c_parent || !c_name)                          /* 097 names them all: a layout renamed */
        return w_fail(why, W_ERR_CORRUPT, "the layouts (097) lack AreaTable's ContinentID, ParentAreaID or AreaName_enUS");
    if (id == 0) return w_fail(why, W_ERR_NOT_FOUND, "area 0 is none");
    TableRow row;
    WWhy inner;
    WErr rc = tables_row(tables, "AreaTable", id, &row, &inner);
    if (rc) return w_fail(why, rc, "area %u: %s", id, inner.text);
    /* a copy narrower than AreaName is no name, not a refused table:
     * 097 leaves the columns unmarked (1109 step 1aa, as decision 180) */
    if (row.dbc->fields < c_name)
        return w_fail(why, W_ERR_NOT_FOUND, "area %u's row (from %s) is %u fields wide, and AreaName is its field %u",
                      id, row.from ? row.from : asset_source_name(row.source), row.dbc->fields, c_name);
    out->id = id;
    out->continent = dbc_u32(row.dbc, row.row, c_continent);
    out->parent = dbc_u32(row.dbc, row.row, c_parent);
    snprintf(out->name, sizeof out->name, "%s", dbc_text(row.dbc, row.row, c_name));
    out->from = row.source;
    return W_OK;
}
/* }}} */

/* {{{ area_chunk_at */
int area_chunk_at(const AdtArea cells[256], double x, double y)
{
    const double side = ADT_CHUNK_SIZE, give = 0.01;
    for (int k = 0; k < 256; ++k)
        if (x <= cells[k].x + give && x >= cells[k].x - side - give && y <= cells[k].y + give && y >= cells[k].y - side - give) return k;
    return -1;
}
/* }}} */

/* {{{ area_here_init */
void area_here_init(AreaHere *a)
{
    memset(a, 0, sizeof *a);
    a->tx = a->ty = -1;
}
/* }}} */

/* {{{ area_here_update */
int area_here_update(AreaHere *a, Resolver *r, MpqChain *chain, Tables *tables, const char *map_name, double x, double y)
{
    uint32_t was = a->id;
    int was_named = a->named;
    int tx, ty;
    maps_tile_of((float)x, (float)y, &tx, &ty);
    if (tx != a->tx || ty != a->ty) {
        a->tx = tx;
        a->ty = ty;
        a->tile_read = 0;
        memset(a->cells, 0, sizeof a->cells);
        if (tx < 0 || ty < 0 || tx > 63 || ty > 63) w_fail(&a->why, W_ERR_NOT_FOUND, "off the map (tile %d, %d)", tx, ty);
        else {
            char path[300];
            snprintf(path, sizeof path, "World\\Maps\\%s\\%s_%d_%d.adt", map_name, map_name, ty, tx);   /* west-east number first */
            AssetAnswer f;
            WWhy w;
            if (asset_read(r, chain, path, ASSET_GROUND, &f, &w)) w_fail(&a->why, W_ERR_NOT_FOUND, "no ground at tile %d, %d: %s", tx, ty, w.text);
            else {
                if (adt_read_areas(f.data, f.len, a->cells, &w)) w_fail(&a->why, w.code, "%s: %s", path, w.text);
                else a->tile_read = 1;
                asset_answer_free(&f);
            }
        }
    }
    int k = a->tile_read ? area_chunk_at(a->cells, x, y) : -1;
    a->id = k < 0 ? 0 : a->cells[k].area;
    if (a->tile_read && k < 0) w_fail(&a->why, W_ERR_NOT_FOUND, "no chunk of tile %d, %d holds %.2f, %.2f", tx, ty, x, y);
    else if (a->tile_read && a->id == 0) w_fail(&a->why, W_ERR_NOT_FOUND, "the chunk names no area");
    if (a->id != was || !a->tile_read) {
        a->named = 0;
        memset(&a->row, 0, sizeof a->row);
        if (a->id && !area_find(tables, a->id, &a->row, &a->why)) a->named = 1;
    }
    return a->id != was || a->named != was_named;
}
/* }}} */
