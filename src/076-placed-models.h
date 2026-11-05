/*
 * 076-placed-models.h (issues 1109 step 1c, 105a, 401, 204) - a placed
 * model's and a building's files, from wherever they are
 *
 * For a general audience: a map's ground files place small models
 * (trees, rocks, lamps: "doodads") and buildings by name, and the
 * doodad drawer (042-doodads.h) loads each name once. A map converted
 * from Warcraft III by world-edit-to-execute (its W02b) names two kinds:
 * the game's own models, by their names in the game
 * (World\Generic\...\Tree.m2), and the map's own WC3 models, which the
 * converter copies into its build folder at their own paths, with the
 * pictures they name (war3mapImported\Deadtree1.mdx). This finds a
 * placed name's files through the asset resolver (060-resolver.h: the
 * player's packs, the server's assets, overlays such as the test map's
 * folder, the player's archives, then a placeholder) and reads them into
 * the one model structure the drawer takes (022-m2.h):
 *
 *   - a name ending in .mdx is first asked for as written, with no
 *     placeholder; bytes that start "MDLX" are a WC3 model, read by
 *     061-mdx.h, its points turned from WC3 units into yards by the map's
 *     own rule (map.lua's wc3_to_wow.scale, 073-converted-map.h), since
 *     the converter keeps WC3's own size in the placement;
 *   - otherwise (the game's own models, which ground files name with
 *     .mdx or .m2 alike, and a pack's .m2 under an .mdx name) the .m2
 *     beside the name, and its first view, <name>00.skin;
 *   - with a resolver, a model no place has stands in as the placeholder
 *     box (said, counted, refused in strict mode), whole: when one of its
 *     two files is found and the other isn't, both are the box's
 *     (asset_stand_in), so no model is drawn with another's view;
 *   - with none (every run but a test map's for now, issue 1109's
 *     decision 4) the archives alone, as the drawer read before: a model
 *     they lack is W_ERR_NOT_FOUND, and left out.
 *
 * A building (021-wmo.h) is its root file and one file per part
 * (<name>_000.wmo ...), read the same way but never as a placeholder:
 * the box is no building's root. The drawer stands the box in for a
 * building no place has (placed_model_stand_in), when there is a resolver.
 *
 * Window-free: in the test program and the executable (it reads through
 * the resolver, which isn't in the reader library).
 */
#ifndef EG_PLACED_MODELS_H
#define EG_PLACED_MODELS_H

#include "013-errors.h"
#include "015-mpq.h"
#include "021-wmo.h"
#include "022-m2.h"
#include "060-resolver.h"
#include "061-mdx.h"

#include <stddef.h>

/* A placed model, read:
 *   wc3       1: a WC3 model (MDX) a map brought; 0: an M2 (the box included)
 *   m2        (an M2) its model
 *   mdx       (WC3) the reader's whole model; its drawable part's points in
 *             yards (its skeleton, pivots and tracks are still WC3's units)
 *   m2_bytes  (an M2) the .m2's bytes, kept for its skeleton, particles and
 *             solid shape (023-anim.h, 024-particles.h, m2_collision); NULL
 *             for a WC3 model, which stands in its rest pose (402 step 4)
 *   base      the name without its ending, for its .anim files
 *   source    where the model's file came from (ASSET_FROM_PLACEHOLDER: the
 *             box stood in), and `from`, which folder or archive (the
 *             resolver's or the chain's words, valid while they live) */
typedef struct {
    int            wc3;
    M2Model        m2;
    MdxModel       mdx;
    unsigned char *m2_bytes;
    size_t         m2_len;
    char           base[260];
    AssetSource    source;
    const char    *from;
} PlacedModel;

/* The model named `name` (as a ground file's MMDX or a building's
 * furnishing names it), through `r` (NULL: the archives alone, `chain`),
 * a WC3 model's points times `wc3_yards` (yards per WC3 unit; 0 or less:
 * 1/30.72, WC3's own grid of 128 units a cell to the game's 4.1667 yards).
 * W_OK with `*out` (freed with placed_model_free); W_ERR_NOT_FOUND
 * (no resolver, the archives lacking a file; or strict mode refusing a
 * placeholder); W_ERR_REFUSED for an empty or too long name; a reader's
 * code (W_ERR_CORRUPT ...) with the name; the resolver's W_ERR_IO. */
WErr placed_model_read(Resolver *r, MpqChain *chain, const char *name, double wc3_yards, PlacedModel *out, WWhy *why);
void placed_model_free(PlacedModel *m);

/* What is drawn: the WC3 model's drawable part, or the M2's. */
const M2Model *placed_model_drawn(const PlacedModel *m);

/* The placeholder box standing in for `name` (a building no place has),
 * said and counted by the resolver (asset_stand_in), with `source`
 * ASSET_FROM_PLACEHOLDER. W_ERR_NOT_FOUND with no resolver or in strict
 * mode. */
WErr placed_model_stand_in(Resolver *r, const char *name, PlacedModel *out, WWhy *why);

/* A building's root file (`name`, as a ground file's MWMO names it), and
 * its part `group` (<name without .wmo>_<group, three digits>.wmo),
 * through `r` (NULL: the archives alone), never a placeholder: W_OK with
 * `*out` (freed with wmo_free_root, wmo_free_group) and, for the root,
 * where it came from in `*source` (may be NULL); W_ERR_NOT_FOUND,
 * quietly, when no place has it; W_ERR_CORRUPT from 021's reader, with
 * the name. */
WErr placed_building_root(Resolver *r, MpqChain *chain, const char *name, WmoRoot *out, AssetSource *source, WWhy *why);
WErr placed_building_group(Resolver *r, MpqChain *chain, const char *name, int group, WmoGroup *out, WWhy *why);

#endif
