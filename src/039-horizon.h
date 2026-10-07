/*
 * 039-horizon.h (issue 173s) - the far horizon, from the map's low-detail heights
 *
 * For a general audience: besides the full ground files, each map keeps
 * one small file for the whole continent (World\Maps\<map>\<map>.wdl):
 * for every tile a coarse 17 x 17 grid of heights, one every 33 yards. The
 * original draws it beyond the loaded tiles so distant mountains show on
 * the horizon. This draws it out to HORIZON_REACH tiles round her,
 * wherever the full ground isn't loaded, shaded in the fog's colour (so the
 * mountains stand as soft silhouettes against the sky).
 *
 * Layout (3.3.5a): MAOF, 64 x 64 file offsets (tile x * 64 + tile y; 0:
 * none), each to an MARE block: 17 x 17 corner heights then 16 x 16 centre
 * heights (signed 16-bit, yards), rows running south and columns east as
 * in the ground files; then MAHO: 16 rows of 16 hole bits (lowest bit the
 * first column).
 */
#ifndef EG_HORIZON_H
#define EG_HORIZON_H

#include "013-errors.h"

#include "026-light.h"
#include "015-mpq.h"
#include "raylib.h"

#define HORIZON_REACH 6               /* tiles round hers the horizon is drawn to */

/* Reads the map's low-detail heights; the chain may be closed afterwards.
 * W_OK, or a code with `why` (W_ERR_NOT_FOUND: a map with no .wdl draws
 * no horizon; said, not a failure). */
WErr  horizon_init(MpqChain *chain, const char *map_name, double ox, double oy, WWhy *why);

/* Draws every low-detail tile within reach of tile (tx, ty) for which
 * `loaded(x, y)` says the full ground isn't there. */
void horizon_draw(int tx, int ty, int (*loaded)(int x, int y), Vector3 eye, const LightNow *light);

/* The low-detail height at the north-west corner of tile (tx, ty), for the
 * tests; below -10000 when the tile has none. */
float horizon_corner(int tx, int ty);

#endif
