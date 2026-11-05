/*
 * 091-model-solids.h (issue 1109 step 1o, 173q) - the placed models'
 * floors and walls, with no window
 *
 * For a general audience: trees, rocks, bridges and buildings stand on a
 * map, each placed by the map's ground files (020-adt.h). Each has a
 * solid shape (simpler than what is drawn: a game model's collision
 * triangles, a building's triangles marked solid), and the mover
 * (029-mover.h) walks on those floors and stops at those walls
 * (028-collide.h). The doodad drawer (042-doodads.h) used to be the only
 * part that built those shapes, so a run with no window walked through
 * every model. This part builds them with no window: the shape of one
 * model or one building part (used by the drawer too, so both build them
 * the same way), and, for a run with no window (089-headless-run.h), the
 * shapes of every model and building a tile places, as the drawer places
 * them:
 *
 *   - each doodad's model through 076-placed-models.h (the resolver, or
 *     the archives alone), read once per name; a game model's collision
 *     triangles (m2_collision); a WC3 model a converted map brought has
 *     none here (its own collision shapes, CLID, are WC3's selection
 *     shapes, not seated: it is walked through, as in the window); the
 *     placeholder box and the baseline set's stand-ins have none either
 *     (they are signs, not walls);
 *   - each building's root and parts (076), its parts' solid triangles
 *     (021-wmo.h's wmo_solid), its pools (filed in the liquid world as
 *     the drawer files them, so she swims in them), and the furnishings
 *     of set 0 and of the set the placement asks for, each a model as
 *     above; a building no place has stands in as the box (no solid);
 *   - a placement listed by two tiles (one unique id) placed once, by the
 *     first tile that meets it; dropping that tile lets the other place
 *     it again, as the drawer does (173s).
 *
 * The floor and walls the mover asks for (model_solids_ground,
 * model_solids_wall) are the windowed client's rule (043-main.c), here so
 * both runs share it: the higher of the map's ground and a model's floor
 * at or below her feet plus a step; walls from the models' steep
 * triangles, her body WORLD_HER_RADIUS round and WORLD_HER_HEIGHT tall.
 *
 * Window-free (CORE: the test program and the executable).
 */
#ifndef EG_MODEL_SOLIDS_H
#define EG_MODEL_SOLIDS_H

#include "013-errors.h"
#include "015-mpq.h"
#include "018-maps.h"
#include "020-adt.h"
#include "021-wmo.h"
#include "027-liquid.h"
#include "028-collide.h"
#include "060-resolver.h"
#include "076-placed-models.h"

#include <stdio.h>

/* {{{ one model's or one building part's solid shape
 * Triangles, 9 floats each (three corners), in the model's own space.
 * model_solids_of_model: a placed model's (an M2's collision lists; none
 * for a WC3 model or the box): W_OK with `*tris` (free()d by the caller,
 * NULL for none) and `*ntri`; W_ERR_CORRUPT naming what was wrong (then
 * none). model_solids_of_group: appends a building part's solid
 * triangles to `*tris` (grown with realloc) and `*ntri`; W_OK, or
 * W_ERR_FULL with no memory (left as it was). */
WErr model_solids_of_model(const PlacedModel *pm, float **tris, int *ntri, WWhy *why);
WErr model_solids_of_group(const WmoGroup *g, float **tris, int *ntri, WWhy *why);

/* The box standing in for a building no place has, sized to the
 * building's box (MODF's extents, in the placement's axes: 0 and 2
 * across, 1 up; a yard at least): `m` (the placement's adt_place_matrix)
 * stretched in place. */
void model_solids_stand_in_size(const AdtPlace *p, float m[16]);
/* }}} */

/* {{{ the mover's floor and walls (043's rule) */
/* The floor at world (x, y) at or below `z_top`: the map's ground (`world`,
 * may be NULL) or a model's floor (`solids`, may be NULL), the model's
 * when it is higher than a map ground at or below z_top, or when the map's
 * ground is above z_top (she is beneath it); its rise per yard in *gx,
 * *gy; below -10000 when there is neither. */
float model_solids_ground(MapWorld *world, CollideWorld *solids, float x, float y, float z_top, float *gx, float *gy);
/* Whether walking from (x0, y0) to (x1, y1), feet at `z`, runs into a
 * model's wall steeper than `max_grad` (rise per yard), her body
 * WORLD_HER_RADIUS round and WORLD_HER_HEIGHT tall, stepping over
 * MOVER_STEP_UP; 1 with the wall's facing in *nx, *ny. 0 with no solids. */
int model_solids_wall(CollideWorld *solids, double x0, double y0, double x1, double y1, double z, float max_grad, float *nx, float *ny);
/* }}} */

/* {{{ every model and building the tiles place (the handle) */
typedef struct ModelSolids ModelSolids;

/* What it has read and placed:
 *   models          doodad and furnishing models read (once a name)
 *   models_solid    those with a solid shape
 *   models_wc3      WC3 models a converted map brought (walked through)
 *   buildings       buildings read (once a name)
 *   stand_ins       models and buildings stood in for: the box, or the
 *                   baseline set's stand-in for a game model (085)
 *   left_out        models and buildings that couldn't be read
 *   copies          copies placed now (doodads, buildings, furnishings)
 *   solid_copies    those with a solid shape
 *   triangles       solid triangles placed now (028's count)
 *   pools           building pools filed now */
typedef struct {
    int models, models_solid, models_wc3, buildings, stand_ins, left_out;
    int copies, solid_copies, triangles, pools;
} ModelSolidsCounts;

/* A new one reading through `assets` (NULL: the archives alone, `chain`),
 * a WC3 model's points times `wc3_yards` (yards per WC3 unit, 0: WC3's
 * own 1/30.72), placing solid shapes in `solids` and pools in `liquids`
 * (either may be NULL), saying what it leaves out on `log` (NULL:
 * stderr). NULL with no memory. */
ModelSolids *model_solids_new(Resolver *assets, MpqChain *chain, double wc3_yards, CollideWorld *solids, LiquidWorld *liquids, FILE *log);
void         model_solids_free(ModelSolids *ms);

/* Places tile `tile`'s (tx * 64 + ty) doodads and buildings, `t` read by
 * adt_read, relative to the origin (ox, oy) the solids were made with
 * (collide_new). A model or building that can't be read is said once and
 * left out. W_OK, or W_ERR_FULL with no memory. */
WErr model_solids_add_tile(ModelSolids *ms, const AdtTile *t, int tile, double ox, double oy, WWhy *why);
/* Forgets tile `tile`'s copies (028's collide_drop_tile; its pools are
 * the liquid world's to drop, liquid_drop_tile), then lets the tiles
 * still placed place what only it had. */
void model_solids_drop_tile(ModelSolids *ms, int tile);
void model_solids_counts(const ModelSolids *ms, ModelSolidsCounts *out);
/* }}} */

#endif
