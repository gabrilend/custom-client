/*
 * 087-world-start.h (issue 1109) - the world's start, before the window
 *
 * For a general audience: everything the client must find before it can
 * open a window on a map, gathered in one place so it can be checked with
 * no window: the original game's archives (in a test-map run they may be
 * missing: the map's own folder and the baseline set answer instead), the
 * resolver of a test-map run (060), the client's tables (074), which
 * folder the map is in (Map.dbc), the ground's heights (the server's map
 * files, or a test map's own ground files, 075) and the ground under her
 * at the start, and the steepest ground she can walk up (the server's
 * navigation tiles, or for a test map with none anywhere, the rule they
 * hold, WORLD_START_SLOPE).
 *
 * 043-main.c opens it and then the window; tests/044 runs it on the
 * converted maps with no archives at all.
 *
 * Window-free (CORE: the test program and the executable).
 */
#ifndef EG_WORLD_START_H
#define EG_WORLD_START_H

#include "013-errors.h"
#include "014-settings.h"
#include "015-mpq.h"
#include "018-maps.h"
#include "060-resolver.h"
#include "067-test-map.h"
#include "073-converted-map.h"
#include "074-tables.h"
#include "075-adt-heights.h"

#include <stdio.h>

/* The steepest walkable ground, degrees, for a test map when no map's
 * navigation tiles are at hand: what the server's own tiles say (its
 * Kalimdor's: the test program checks them when they are there), since
 * the server builds every map by one rule (1109, decision 92). */
#define WORLD_START_SLOPE 60.0f

/* Her, as every start makes her (shared by 043's window and 089's
 * headless run, so both start her the same): facing the town's west side
 * at Astranaar (radians, the game's: 0 north, rising west), 120% of the
 * base speeds (the owner, 2026-10-04), a night-elf woman (ChrRaces 4,
 * sex 1) in a fresh hunter's outfit (ChrClasses 3; CharStartOutfit).
 * Her body against the models' walls (173q; 091-model-solids.h): 0.4
 * yards round, 2.3 tall (the original's own numbers are 173q's open
 * question). */
#define WORLD_START_FACING 3.6
#define WORLD_START_SPEED  1.2
#define WORLD_HER_RACE     4
#define WORLD_HER_SEX      1
#define WORLD_HER_CLASS    3
#define WORLD_HER_RADIUS   0.4f
#define WORLD_HER_HEIGHT   2.3f

/* What the start is given:
 *   client      the original game's Data folder (its archives)
 *   maps        the server's maps folder (mmaps is beside it)
 *   assets_dir  the profile's assets folder (packs, overlays, the WC3
 *               install, the baseline set); "" or NULL for none
 *   settings    the player's settings (the resolver's places)
 *   test_map    the editor's test map (067), or NULL for an ordinary run
 *   conv        the test map's map.lua (073), with test_map
 *   map_id, start_x, start_y   the map and where she starts on it
 *   log         where what was found is said (the client's terminal) */
typedef struct {
    const char *client, *maps, *assets_dir;
    const Settings *settings;
    const TestMap *test_map;
    const ConvertedMap *conv;
    int    map_id;
    double start_x, start_y;
    FILE  *log;
} WorldStartIn;

/* What it found, owned until world_start_close:
 *   chain       the game's archives; none open (count 0) in a test-map
 *               run with no install
 *   install     1 when the archives are open
 *   assets      a test-map run's resolver (its folder first of the
 *               overlays, the baseline set after the installs), or NULL
 *   tables      the client's tables, per row (through `assets` when open)
 *   map_name    the map's folder under World\Maps (Map.dbc's row)
 *   world       the ground's heights; adt_heights its source when they
 *               come from the map's own ground files (else NULL)
 *   z           the ground at the start
 *   max_slope   degrees; slope_from says where it came from */
typedef struct {
    MpqChain    chain;
    int         install;
    Resolver   *assets;
    Tables     *tables;
    char        map_name[64];
    MapWorld    world;
    AdtHeights *adt_heights;
    float       z, max_slope;
    char        slope_from[200];
} WorldStart;

/* Opens it all, in that order. W_OK; otherwise the first failure's code
 * with `why` saying it (no archives in an ordinary run, a broken pack or
 * baseline set, no Map row, a test map whose Map row names another
 * folder, no ground at the start, no navigation tiles in an ordinary
 * run), and whatever was opened closed. */
WErr world_start_open(const WorldStartIn *in, WorldStart *out, WWhy *why);
/* Lets it go, in the order the parts depend on each other (heights,
 * tables, resolver, archives). */
void world_start_close(WorldStart *w);

#endif
