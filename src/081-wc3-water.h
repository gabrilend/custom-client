/*
 * 081-wc3-water.h (issues 302, 1109) - a converted map's water as
 * Warcraft III draws it, without the window
 *
 * For a general audience: world-edit-to-execute's converter turns a WC3
 * map's water into the game's own water (ground files' liquid, kind 1:
 * plain water), drawn today with the game's lake pictures and the zone's
 * colours (040). A player who has Warcraft III can see the map's own
 * water instead: WC3's rippling pictures, coloured by how deep the water
 * is at each tile point, from pale shallows to dark depths. This reads
 * what that needs, so the water drawer can use it (issue 1109's water
 * plan: this is its first step; 040 doesn't use it yet).
 *
 * What WC3 reads (the published map viewers' reading, HiveWE's): the
 * sheet TerrainArt\Water.slk, its row named by the map's main tileset's
 * letter and "Sha" ("LSha" for Lordaeron summer), with these columns:
 *   texFile   the pictures' path less the frame: frame k is texFile, k
 *             in two digits, ".blp" (ReplaceableTextures\Water\Water00.blp)
 *   numTex    how many frames, shown one after another (the ripples)
 *   texRate   frames a second
 *   height    the drawn surface's offset from the stored water level, in
 *             cells (128 WC3 units); read and reported, not used: the
 *             surface stays 063's (decision 66)
 *   Smin_A, Smin_R, Smin_G, Smin_B, Smax_..., Dmin_..., Dmax_...: the
 *             colours (0 to 255, alpha first) shallow water goes between,
 *             and deep water
 * The colour at a point by its depth d (water z less ground z, 063's, in
 * cells): up to 64/128 shallow, from Smin at 10/128 (and less) to Smax at
 * 64/128; past that deep, from Dmin at 64/128 to Dmax at 72/128 (and
 * more). The map's own water tint (war3map.w3i) isn't read: the
 * converter doesn't copy that file to the build (the plan's step 3).
 *
 * Where the files come from: the sheet and the frames through the
 * resolver (060): the player's packs, the server's, overlays (the map's
 * own imports), the player's Warcraft III archives; never the checker.
 * With no sheet, no row for the tileset, or a frame no place has, there
 * is no WC3 water and the converter's stays.
 *
 * Window-free: the test program and the executable. Opening reads and
 * decodes everything once; after it the handle is only read.
 */
#ifndef EG_WC3_WATER_H
#define EG_WC3_WATER_H
#include "013-errors.h"
#include "015-mpq.h"
#include "060-resolver.h"
#include "063-w3e.h"
#include <stdint.h>

/* The depths of WC3's colour rule, in cells (128 WC3 units). */
#define WC3_WATER_MIN_DEPTH   (10.0 / 128)
#define WC3_WATER_DEEP_LEVEL  (64.0 / 128)
#define WC3_WATER_MAX_DEPTH   (72.0 / 128)
#define WC3_WATER_FRAMES_MAX  100          /* two digits a frame number */

/* The map's water, from its tileset's row. Field by field:
 *   row          the sheet's row ("LSha"), ended by a zero
 *   tex_file     texFile, as the sheet gives it
 *   frames       numTex, 1 to WC3_WATER_FRAMES_MAX
 *   rate         texRate, frames a second (above 0; 0 in the sheet: 1)
 *   height       the sheet's height, in cells (0 when it gives none)
 *   shallow_min, shallow_max, deep_min, deep_max   RGBA, 0 to 255 */
typedef struct {
    char    row[8];
    char    tex_file[200];
    int     frames;
    double  rate;
    double  height;
    uint8_t shallow_min[4], shallow_max[4], deep_min[4], deep_max[4];
} Wc3WaterKind;

/* One frame: its path (tex_file, two digits, ".blp"), where it came
 * from, and its pixels (width x height RGBA, rows from the top). */
typedef struct {
    char           path[208];
    AssetSource    source;
    int            width, height;
    unsigned char *rgba;
} Wc3WaterFrame;

/* What the map's points come to. */
typedef struct {
    int wet;            /* points 063 says are wet (w3e_is_wet) */
    int shallow;        /* of them, no deeper than WC3_WATER_DEEP_LEVEL */
    int deep;           /* the rest */
    double deepest;     /* the greatest depth, in cells */
} Wc3WaterCounts;

typedef struct Wc3Water Wc3Water;

/* Reads Water.slk and every frame of the map's tileset's row through `r`
 * (a test-map run's resolver), for the map's ground `g` (063: its main
 * tileset's letter, its points; it must outlive the handle). W_OK with
 * `*out` (freed by wc3_water_free); W_ERR_NOT_FOUND, with why, when there
 * is no resolver, no Water.slk in any place (no Warcraft III install),
 * no row for the tileset, or a frame no place has (naming it): the
 * converter's water stays; W_ERR_CORRUPT for a sheet 077 refuses, a row
 * whose texFile is empty or numTex outside 1 to WC3_WATER_FRAMES_MAX, a
 * colour outside 0 to 255, or a frame 017 can't decode (naming it). */
WErr wc3_water_open(Resolver *r, MpqChain *chain, const W3eGround *g, Wc3Water **out, WWhy *why);
void wc3_water_free(Wc3Water *w);

const Wc3WaterKind  *wc3_water_kind(const Wc3Water *w);
const Wc3WaterFrame *wc3_water_frame(const Wc3Water *w, int k);    /* NULL outside 0 to frames - 1 */
Wc3WaterCounts       wc3_water_counts(const Wc3Water *w);

/* The frame shown `seconds` after the start: floor(seconds x rate),
 * round the frames (0 for a time below 0). */
int  wc3_water_frame_at(const Wc3Water *w, double seconds);

/* WC3's colour for water `depth` cells deep (the rule above), RGBA. */
void wc3_water_colour(const Wc3WaterKind *k, double depth, uint8_t out[4]);

/* Tile point (i, j) (rows from the south): 1 when wet, with its depth in
 * cells (063's water z less ground z, over 128) and its colour when
 * asked; 0 when dry or outside the grid. */
int  wc3_water_point(const Wc3Water *w, int i, int j, double *depth, uint8_t rgba[4]);

#endif
