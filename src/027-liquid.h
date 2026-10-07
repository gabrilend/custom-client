/*
 * 027-liquid.h (issue 173r) - where the water is, for the mover
 *
 * For a general audience: keeps every water layer of every loaded tile
 * (020-adt.h's AdtLiquid), filed by chunk, and answers "how high is the
 * water's surface here, and what kind of water is it?". The surface
 * between a cell's four corners is blended, as the ground's is.
 * Buildings' pools (021-wmo.h's WmoPool) are kept too, each with its
 * building's placement: a spot is turned into the building's own space,
 * its pool cell found, and the surface's height turned back into the
 * world's.
 */
#ifndef EG_LIQUID_H
#define EG_LIQUID_H

#include "020-adt.h"
#include "021-wmo.h"

typedef struct LiquidWorld LiquidWorld;

LiquidWorld *liquid_new(void);
void         liquid_free(LiquidWorld *w);

/* Files tile (tx, ty)'s water layers (from its ground file). */
void  liquid_add_tile(LiquidWorld *w, int tx, int ty, const AdtTile *t);

/* Files a building's pool, of kind `kind` (LiquidType.dbc id), placed by
 * `at` (the building's placement, Raylib's layout, relative to the
 * drawing origin `ox`, `oy`), brought by map tile `tile` (tx * 64 + ty).
 * The pool's grid is copied. */
void  liquid_add_pool(LiquidWorld *w, int tile, const float at[16], double ox, double oy, const WmoPool *p, int kind);

/* Forgets tile (tx, ty)'s water, its pools included (the tile is dropped, 173s). */
void  liquid_drop_tile(LiquidWorld *w, int tx, int ty);

/* The highest water surface at world (x, y): its height, and its kind
 * (LiquidType.dbc id) in *kind; below -10000 when there is no water. */
float liquid_surface(const LiquidWorld *w, double x, double y, int *kind);

#endif
