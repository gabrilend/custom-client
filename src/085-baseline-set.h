/*
 * 085-baseline-set.h (issues 105a, 1109) - the smallest baseline set,
 * made from what the repositories own
 *
 * For a general audience: the resolver (060) asks a fifth place before it
 * gives up and draws a placeholder: the baseline set, the project's own
 * stand-ins under the original game's names, so a player without
 * Blizzard's files sees ours rather than a magenta checker or a
 * wire-frame (the owner, 2026-11-04: "we're going to have sensible
 * defaults, and if users want, they can plug in their Blizzard assets").
 * The real set is the asset forge's (world-edit-to-execute W05, W06), not
 * made yet. This makes the smallest one that lets a converted Warcraft III
 * map draw with no install, from nothing but the converter's own tables
 * and code here (issue 1109: decisions 81 to 83 place it in the
 * resolver, 84 to 88 say what it holds):
 *
 *   ground   every ground picture the converter can name (its
 *            ground_textures.lua: each tileset's grounds and blight, and
 *            the fallback) as a 64 x 64 picture of that row's own colour
 *            (its `rgb`), gently mottled so it reads as ground; the same
 *            path always gives the same picture
 *   models   every doodad model the converter can name (its models.lua,
 *            the `doodads` part: types, by_model, classes) as one stand-in
 *            model: a grey eight-sided post, a yard across and two high,
 *            with its own picture (BASELINE_STAND_IN_PICTURE). Buildings
 *            (.wmo rows) and the units' creature models are left out and
 *            counted: a building is a root and its groups, a creature
 *            needs a skeleton to move
 *   water    the converter's one liquid, LiquidType 1 (water), as the
 *            run of frames 040 asks for, BASELINE_WATER_FRAMES of them,
 *            named by BASELINE_WATER_PATTERN (3.3.5a's name, recalled:
 *            unchecked here, with no client)
 *
 * The folder is laid out as a pack (059-loose-folders.h): every file
 * beside a manifest.lua entry giving its provenance (route "hand": made
 * by this code; lineage "independent"; no similarity score yet). Each
 * picture lies at its own name, folded (tileset/ashenvale/...); the
 * models share one file pair.
 *
 * Window-free (CORE: the test program, the executable and make-baseline).
 */
#ifndef EG_BASELINE_SET_H
#define EG_BASELINE_SET_H
#include "013-errors.h"

#include <stddef.h>
#include <stdint.h>

#define BASELINE_PICTURE_SIDE     64
#define BASELINE_WATER_PATTERN    "XTextures\\river\\lake_a.%d.blp"   /* frames 1 upward, as LiquidType 1 names them */
#define BASELINE_WATER_FRAMES     30
#define BASELINE_STAND_IN_PICTURE "Baseline\\StandIn.blp"            /* the set's own name, not the original game's */
#define BASELINE_STAND_IN_MODEL   "baseline/standin.m2"              /* the models' shared files in the folder */
#define BASELINE_STAND_IN_SKIN    "baseline/standin00.skin"

/* What a make wrote, counted:
 *   grounds        ground pictures (one a different path)
 *   models         doodad paths given the stand-in (each its .m2 and 00.skin)
 *   water_frames   frames of the water's run
 *   files, entries files written (the manifest left out) and manifest entries
 *   same_path      rows naming a path an earlier row named (the first's colour kept)
 *   buildings      .wmo rows left out
 *   units          unit rows left out (creature models)
 *   other          rows naming another kind of file, left out */
typedef struct {
    int grounds, models, water_frames, files, entries;
    int same_path, buildings, units, other;
} BaselineMade;

/* Makes the set in `folder` (made when it isn't there) from the
 * converter's ground table `ground_table` and model table `model_table`
 * (world-edit-to-execute's src/wowbuild/ground_textures.lua and
 * models.lua: Lua data, read by 059). W_OK with `*made`; the tables'
 * own refusals (059's codes, naming the file); W_ERR_REFUSED for a row
 * whose `wow` is empty or not text, a ground row whose `rgb` isn't three
 * numbers from 0 to 255 (naming the file and line), and a folder that
 * holds files but no manifest.lua (not a set this made: left alone);
 * W_ERR_IO when a file can't be written. */
WErr baseline_set_make(const char *folder, const char *ground_table, const char *model_table, BaselineMade *made, WWhy *why);

/* The pieces, for the tests (each malloc'd, freed by the caller):
 * a ground picture (BLP2, plain BGRA, BASELINE_PICTURE_SIDE square, no
 * smaller copies) of colour `rgb` mottled by `seed`; the water's frame
 * `frame` (1 to BASELINE_WATER_FRAMES; the run repeats seamlessly); the
 * stand-in's picture; the stand-in model (M2 version 264, naming
 * BASELINE_STAND_IN_PICTURE) and its .skin. */
unsigned char *baseline_ground_picture(const uint8_t rgb[3], uint32_t seed, size_t *len);
unsigned char *baseline_water_frame(int frame, size_t *len);
unsigned char *baseline_stand_in_picture(size_t *len);
unsigned char *baseline_stand_in_model(size_t *len);
unsigned char *baseline_stand_in_skin(size_t *len);

/* SHA-256 of `len` bytes as 64 small hex digits (a manifest entry's id). */
void baseline_sha256_hex(const void *data, size_t len, char out[65]);

#endif
