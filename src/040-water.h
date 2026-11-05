/*
 * 040-water.h (issue 173r) - drawing the water
 *
 * For a general audience: each ground file's water layers (020-adt.h) become
 * flat-ish meshes over their wet cells, at the surface heights the file
 * gives. Each kind of water (LiquidType.dbc) has a numbered run of
 * pictures (lake_a.1.blp, lake_a.2.blp ...) shown one after another so
 * the surface ripples. Water and the sea are tinted by the zone's water
 * colours (026-light.h: river and ocean, near and deep, by time of day) and
 * drawn see-through over the ground, after the solid world; lava and
 * slime are drawn as their pictures, unlit and solid. A converted map's
 * water, when the player has Warcraft III (081), is drawn as WC3 draws
 * it: WC3's own run of pictures, each point coloured by its depth and
 * standing at WC3's own surface (082 works the points out and holds the
 * shader's text; issue 1109's water plan).
 */
#ifndef EG_WATER_H
#define EG_WATER_H

#include "013-errors.h"

#include "020-adt.h"
#include "021-wmo.h"
#include "026-light.h"
#include "036-texture.h"
#include "081-wc3-water.h"
#include "015-mpq.h"
#include "060-resolver.h"
#include "074-tables.h"
#include "raylib.h"

/* A water drawer (the handle: docs/001, "Handles"), its kinds' rows from
 * `tables` (LiquidType per row, 074: a converted map's own, the
 * archives', then the baseline set's, which names the set's frames;
 * 1109 step 1l); the chain and tables must outlive it. Its pictures are asked of
 * `assets` (a test-map run's resolver, 060: so the baseline set's frames
 * answer where no install has them; NULL for the archives alone), never
 * as a placeholder: a kind's run ends at its first missing frame. NULL
 * with `why` when the table or the shader can't be had. Freed with
 * water_free. */
typedef struct Water Water;
Water *water_new(MpqChain *chain, Resolver *assets, Tables *tables, const Textures *textures, WWhy *why);
void   water_free(Water *w);

/* Draws water and sea layers added from now on as `wc3`'s (a converted
 * map's WC3 water; it must outlive the drawer), its frames sent to the
 * card now; NULL goes back to the game's own water. Layers already added
 * stay as they were (043 calls it before the first tile). */
void water_use_wc3(Water *w, const Wc3Water *wc3);

/* Adds a tile's water (tile = tx * 64 + ty), placed relative to the
 * drawing origin; drops it again (173s). */
void water_add_tile(Water *w, const AdtTile *t, int tile, double ox, double oy);
void water_drop_tile(Water *w, int tile);

/* Adds a building's pool (021-wmo.h), of kind `kind` (LiquidType.dbc id),
 * placed by `at` (the building's placement, Raylib's layout, relative to
 * the drawing origin), brought by `tile`; water_drop_tile drops it. Its
 * wet cells are drawn as the ground's water is, the picture repeating
 * once a cell. */
void water_add_pool(Water *w, int tile, const float at[16], const WmoPool *p, int kind);

/* Draws all water at `seconds` since start (the ripples' clock). */
void water_draw(Water *w, Vector3 eye, const LightNow *light, double seconds);

#endif
