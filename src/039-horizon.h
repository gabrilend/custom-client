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
#include "060-resolver.h"
#include "raylib.h"

#define HORIZON_REACH 6               /* tiles round hers the horizon is drawn to */

/* A horizon (the handle: docs/001, "Handles") from the map's low-detail
 * heights, read through `assets` (NULL: the chain alone); the chain and
 * resolver may be closed afterwards. Always a horizon: when the
 * heights can't be had (W_ERR_NOT_FOUND: a map with no .wdl) it draws
 * nothing, and `why` says why (W_OK when all is well); a map without one
 * is not a failure. Freed with horizon_free. */
typedef struct Horizon Horizon;
Horizon *horizon_new(MpqChain *chain, Resolver *assets, const char *map_name, double ox, double oy, WWhy *why);
void     horizon_free(Horizon *h);

/* Draws every low-detail tile within reach of tile (tx, ty) whose full
 * ground isn't loaded: `loaded` is 64 x 64 bytes (tile x * 64 + y), 1
 * where the full ground is drawn instead. */
void horizon_draw(Horizon *h, int tx, int ty, const char *loaded, Vector3 eye, const LightNow *light);

/* The low-detail height at the north-west corner of tile (tx, ty), for the
 * tests; below -10000 when the tile has none. */
float horizon_corner(const Horizon *h, int tx, int ty);

#endif
