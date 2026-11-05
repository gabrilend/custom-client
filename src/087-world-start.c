/*
 * 087-world-start.c (issue 1109) - the world's start; see 087-world-start.h
 *
 * For a general audience: moved here from 043-main.c's start, unchanged
 * but for three things a test map may now lack: the game's archives (it
 * runs from its own folder and the baseline set), the server's map files
 * (its own ground files give the heights, as before) and every map's
 * navigation tiles (the server's one rule stands in).
 */
#include "087-world-start.h"
#include "019-navmesh-slope.h"
#include "016-dbc.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

static WErr fail(WorldStart *w, WErr rc, WWhy *why) { world_start_close(w); return rc ? rc : why->code; }

WErr world_start_open(const WorldStartIn *in, WorldStart *w, WWhy *why)
{
    memset(w, 0, sizeof *w);
    FILE *log = in->log ? in->log : stdout;
    WErr rc;
    /* {{{ the archives, and in a test-map run the resolver in front of them */
    WWhy inner;
    rc = mpq_chain_open(&w->chain, in->client ? in->client : "", "enUS", &inner);
    if (rc == W_OK) w->install = 1;
    else if (rc == W_ERR_NOT_FOUND && in->test_map)
        fprintf(log, "no WoW install: %s, so the test map's tables, pictures and models come from its own folder and the baseline set, and what neither has stands in\n", inner.text);
    else { mpq_chain_close(&w->chain); return w_fail(why, rc, "%s", inner.text); }
    if (in->test_map) {
        ResolverPlace place;
        if ((rc = test_map_place(in->test_map, in->settings, in->assets_dir ? in->assets_dir : "", w->install ? &w->chain : NULL, &place, why))) return fail(w, rc, why);
        place.log = log;
        if ((rc = resolver_open(&place, &w->assets, why))) return fail(w, rc, why);
        fprintf(log, "test map: %s, the first of %d overlay%s\n", test_map_folder(in->test_map), place.noverlays, place.noverlays == 1 ? "" : "s");
        if (!w->install && !resolver_baseline(w->assets))
            fprintf(log, "test map: no install and no baseline set: what its own folder lacks stands in as a placeholder, and a table it lacks stops the start\n");
    }
    w->tables = tables_new(w->assets, &w->chain);
    /* }}} */
    /* {{{ the map's folder (Map.dbc: 1 id, 2 its folder under World\Maps), a converted map's own row first */
    {
        TableRow mr;
        if ((rc = tables_row(w->tables, "Map", (uint32_t)in->map_id, &mr, &inner))) { w_fail(why, rc, "Map.dbc: %s", inner.text); return fail(w, rc, why); }
        snprintf(w->map_name, sizeof w->map_name, "%s", dbc_text(mr.dbc, mr.row, 2));
        if (in->test_map && strcasecmp(w->map_name, in->conv->directory)) {
            w_fail(why, W_ERR_REFUSED, "the test map's %s names map %d's folder %s, but its Map.dbc row (from %s, %s) names %s",
                   CONVERTED_MAP_FILE, in->map_id, in->conv->directory, asset_source_name(mr.source), mr.from, w->map_name);
            return fail(w, W_ERR_REFUSED, why);
        }
        if (in->test_map) fprintf(log, "test map: map %d's row from %s (%s)\n", in->map_id, asset_source_name(mr.source), mr.from);
    }
    /* }}} */
    /* {{{ the ground's heights: the server's map files; in a test-map run
     * whose map the server has none for yet (world-edit-to-execute's W02d
     * writes them), the map's own ground files, made into the server's
     * tiles as its extractor would (075-adt-heights.h) */
    int from_adts = 0;
    if (in->test_map) {
        int tx0, ty0;
        WWhy no_tile;
        maps_tile_of((float)in->start_x, (float)in->start_y, &tx0, &ty0);
        MapTile *t = maps_load_tile(in->maps ? in->maps : "", in->map_id, tx0, ty0, &no_tile);
        if (t) maps_free_tile(t);
        else if (no_tile.code == W_ERR_NOT_FOUND) from_adts = 1;
        else { w_fail(why, no_tile.code, "the server's map files for map %d: %s", in->map_id, no_tile.text); return fail(w, no_tile.code, why); }
    }
    if (from_adts) {
        if ((rc = adt_heights_open(w->assets, &w->chain, w->map_name, &w->adt_heights, why))) return fail(w, rc, why);
        char what[300];
        snprintf(what, sizeof what, "map %d's own ground files (World\\Maps\\%s)", in->map_id, w->map_name);
        maps_world_init_source(&w->world, in->map_id, adt_heights_tile, w->adt_heights, what);
        fprintf(log, "ground: the server's map files (%s) have none for map %d yet; her ground's heights come from %s\n", in->maps ? in->maps : "none given", in->map_id, what);
    } else maps_world_init(&w->world, in->maps ? in->maps : "", in->map_id);
    float gx, gy;
    w->z = maps_world_height(&w->world, (float)in->start_x, (float)in->start_y, &gx, &gy);
    if (w->z < -10000) {
        w_fail(why, W_ERR_NOT_FOUND, "no ground at the start (%.1f, %.1f) in map %d from %s%s", in->start_x, in->start_y, in->map_id, w->world.dir,
               in->test_map ? "" : ": is the maps folder right?");
        return fail(w, W_ERR_NOT_FOUND, why);
    }
    /* }}} */
    /* {{{ the steepest walkable ground, from the server's navigation tiles
     * (data-files/mmaps beside data-files/maps) */
    char mmaps[600];
    snprintf(mmaps, sizeof mmaps, "%s", in->maps ? in->maps : "");
    char *cut = strstr(mmaps, "/maps");
    if (cut) strcpy(cut, "/mmaps");
    else if (!in->test_map) { w_fail(why, W_ERR_REFUSED, "the maps folder %s doesn't end in /maps; can't find mmaps beside it", mmaps); return fail(w, W_ERR_REFUSED, why); }
    w->max_slope = cut ? navmesh_slope_read(mmaps, in->map_id, &inner) : -1;
    if (w->max_slope >= 0) snprintf(w->slope_from, sizeof w->slope_from, "map %d's navigation tiles", in->map_id);
    /* a test map has none until W02d makes them: the server's rule is one
     * rule for every map it builds, so another map's tiles say it */
    static const int STOCK_MAPS[] = { 1, 0, 530, 571 };
    for (int k = 0; cut && w->max_slope < 0 && in->test_map && k < (int)(sizeof STOCK_MAPS / sizeof STOCK_MAPS[0]); ++k) {
        WWhy other;
        w->max_slope = navmesh_slope_read(mmaps, STOCK_MAPS[k], &other);
        if (w->max_slope >= 0) {
            snprintf(w->slope_from, sizeof w->slope_from, "map %d's navigation tiles (the server's rule; map %d has none yet)", STOCK_MAPS[k], in->map_id);
            fprintf(log, "steep ground: map %d has no navigation tiles yet; the server's rule from map %d's: %.1f degrees\n", in->map_id, STOCK_MAPS[k], w->max_slope);
        }
    }
    if (w->max_slope < 0 && in->test_map) {
        w->max_slope = WORLD_START_SLOPE;
        snprintf(w->slope_from, sizeof w->slope_from, "the server's rule as its tiles hold it (no navigation tiles at %.120s)", cut ? mmaps : "no maps folder");
        fprintf(log, "steep ground: no navigation tiles for any map at %s; the server's rule, %.1f degrees\n", cut ? mmaps : "(no maps folder)", w->max_slope);
    }
    if (w->max_slope < 0) { w_fail(why, inner.code ? inner.code : W_ERR_NOT_FOUND, "%s", inner.text); return fail(w, why->code, why); }
    /* }}} */
    return W_OK;
}

void world_start_close(WorldStart *w)
{
    maps_world_free(&w->world);                             /* before the heights' source it may ask */
    adt_heights_free(w->adt_heights);
    tables_free(w->tables);                                 /* before the resolver and archives it reads from */
    resolver_free(w->assets);                               /* before the archives it reads from */
    mpq_chain_close(&w->chain);
    memset(w, 0, sizeof *w);
}
