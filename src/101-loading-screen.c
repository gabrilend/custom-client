/*
 * 101-loading-screen.c (issue 1109 step 1y) - see 101-loading-screen.h
 *
 * For a general audience: three lookups and a decode. The map's row
 * names a loading screen's row, that row names a picture, and the
 * picture is read from the first place that has it and decoded. Each
 * step that finds nothing says which step and which number.
 */
#include "101-loading-screen.h"

#include "016-dbc.h"
#include "097-table-layouts.h"

#include <stdio.h>
#include <string.h>

/* {{{ static column: a column of `table` by name, from the layout (097) */
static uint32_t column(const char *table, const char *name)
{
    return table_layout_find(table_layout(table), name);
}
/* }}} */

/* {{{ static levels_in: how many copies a BLP's file holds (017 decodes
 * each; the first that isn't there ends the count) */
static int levels_in(const unsigned char *s, size_t len)
{
    int n = 0;
    for (; n < 16; ++n) {
        BlpImage img;
        WWhy w;
        if (blp_decode_level(s, len, n, &img, &w)) break;
        blp_free(&img);
    }
    return n;
}
/* }}} */

/* {{{ loading_screen_find */
WErr loading_screen_find(Tables *tables, Resolver *r, MpqChain *chain, int map_id, LoadingScreen *out, WWhy *why)
{
    memset(out, 0, sizeof *out);
    uint32_t c_id = column("Map", "LoadingScreenID"), c_name = column("LoadingScreens", "Name"),
             c_file = column("LoadingScreens", "FileName"), c_wide = column("LoadingScreens", "HasWideScreen");
    if (!c_id || !c_name || !c_file || !c_wide)                        /* 097 names them all: a layout renamed */
        return w_fail(why, W_ERR_CORRUPT, "the layouts (097) lack Map's LoadingScreenID or LoadingScreens' Name, FileName or HasWideScreen");

    TableRow row;
    WWhy inner;
    WErr rc = tables_row(tables, "Map", (uint32_t)map_id, &row, &inner);
    if (rc) return w_fail(why, rc, "no loading screen: %s", inner.text);
    /* a Map copy narrower than the column is no loading screen, not a
     * refused table: 097 leaves the column unmarked so 074 takes such a
     * copy for the rest (1109 step 1y) */
    if (row.dbc->fields < c_id)
        return w_fail(why, W_ERR_NOT_FOUND, "no loading screen: map %d's row (from %s) is %u fields wide, and LoadingScreenID is its field %u",
                      map_id, row.from ? row.from : asset_source_name(row.source), row.dbc->fields, c_id);
    out->id = dbc_u32(row.dbc, row.row, c_id);
    if (out->id == 0) return w_fail(why, W_ERR_NOT_FOUND, "map %d names no loading screen (its LoadingScreenID is 0)", map_id);

    rc = tables_row(tables, "LoadingScreens", out->id, &row, &inner);
    if (rc) return w_fail(why, rc, "map %d's loading screen %u: %s", map_id, out->id, inner.text);
    snprintf(out->name, sizeof out->name, "%s", dbc_text(row.dbc, row.row, c_name));
    snprintf(out->file, sizeof out->file, "%s", dbc_text(row.dbc, row.row, c_file));
    out->wide = dbc_u32(row.dbc, row.row, c_wide);
    out->row_from = row.source;
    if (!out->file[0]) return w_fail(why, W_ERR_NOT_FOUND, "loading screen %u (map %d's) names no picture (its FileName is empty)", out->id, map_id);

    AssetAnswer a;
    rc = asset_read(r, chain, out->file, ASSET_INTERFACE, &a, &inner);
    if (rc == W_ERR_NOT_FOUND) return w_fail(why, rc, "loading screen %u (map %d's): no place has its picture %s", out->id, map_id, out->file);
    if (rc) return w_fail(why, rc, "loading screen %u (map %d's): %s", out->id, map_id, inner.text);
    out->picture_from = a.source;
    rc = blp_decode(a.data, a.len, &out->picture, &inner);
    if (!rc) out->levels = levels_in(a.data, a.len);
    asset_answer_free(&a);
    if (rc) { memset(&out->picture, 0, sizeof out->picture); return w_fail(why, rc, "loading screen %u's picture %s: %s", out->id, out->file, inner.text); }
    return W_OK;
}
/* }}} */

/* {{{ loading_screen_free */
void loading_screen_free(LoadingScreen *s)
{
    if (s->picture.rgba) blp_free(&s->picture);
    memset(s, 0, sizeof *s);
}
/* }}} */
