/*
 * 094-wc3-pathing.h (issue 1109 step 1p) - a converted map's WC3 pathing
 * map, as walls
 *
 * For a general audience: a Warcraft III map keeps, beside its ground
 * (war3map.w3e), a pathing map (war3map.wpm): one byte for every 32-unit
 * square of the map, saying whether walkers, fliers and builders may be
 * there, whether it is blighted, and whether it is land or water. WC3
 * moves its units by it (its cliffs' edges, its buildings' and trees'
 * footprints, the map's closed border), not by its models' shapes. A map
 * converted by world-edit-to-execute (its W02) keeps the file in its
 * build folder (since its draft PR #252), and this reads it through the
 * resolver (060-resolver.h) and answers the mover's wall question with
 * it (091-model-solids.h), in the window and in the run with no window
 * alike.
 *
 * The file (as world-edit-to-execute reads and writes it:
 * src/demo/wc3map/footprint.lua's parse_wpm, src/editor/new_map.lua and
 * save.lua): "MP3W", version (i32, 0), width and height (i32: cells
 * across, west to east, and up, south to north; four a WC3 tile), then
 * width x height bytes, a row at a time from the south. A cell's bits
 * (footprint.lua's names):
 *   0x02  no walking            0x04  no flying
 *   0x08  no building           0x20  blight
 *   0x40  land, not water
 * 0x80 is set on most cells walkers can't enter (cliffs, the closed
 * border: new_map.lua writes 0xCE there) but no parser here or in
 * world-edit-to-execute names it: unchecked, and not read. 0x01 and 0x10
 * are not seen in the two test maps.
 *
 * The grid's south-west corner is the ground's (war3map.w3e's offset_x,
 * offset_y), so cell (i, j) is the 32-unit square from WC3's
 * (offset_x + 32 i, offset_y + 32 j); WC3's units go to the converted
 * ground's yards by map.lua's rule (073-converted-map.h: wow_x = x0 +
 * scale * wc3_y, wow_y = y0 - scale * wc3_x).
 *
 * A wall (wc3_pathing_blocks) is a cell walkers may not enter that is
 * land: deep water is closed to WC3's walkers too, but in the converted
 * world it is water deep enough to swim in, which the mover's water
 * decides. Outside the grid there is no pathing (the ground's own extent
 * decides there: 1109's decision 117).
 *
 * Window-free (CORE: the test program and the executable).
 */
#ifndef EG_WC3_PATHING_H
#define EG_WC3_PATHING_H

#include "013-errors.h"
#include "015-mpq.h"
#include "060-resolver.h"

#include <stddef.h>
#include <stdint.h>

#define WPM_NAME          "war3map.wpm"   /* at the build folder's root, where the converter copies it */
#define WPM_CELL          32              /* WC3 units a cell's side */
#define WPM_NO_WALK       0x02
#define WPM_NO_FLY        0x04
#define WPM_NO_BUILD      0x08
#define WPM_BLIGHT        0x20
#define WPM_LAND          0x40

typedef struct Wc3Pathing Wc3Pathing;

/* Reads a pathing map's bytes: W_OK with `*out` (freed with
 * wc3_pathing_free), placed with its south-west corner at WC3's
 * (offset_x, offset_y) and WC3's units turned into yards by (scale, x0,
 * y0) as above; W_ERR_CORRUPT for bytes that aren't "MP3W", a header or
 * cells cut short, or a width or height below 1; W_ERR_UNSUPPORTED for a
 * version other than 0; W_ERR_REFUSED for a scale of 0 or less. */
WErr wc3_pathing_read(const unsigned char *s, size_t len, double offset_x, double offset_y,
                      double scale, double x0, double y0, Wc3Pathing **out, WWhy *why);

/* The converted map's own: war3map.wpm and war3map.w3e (for its grid's
 * corner) through `r` (`chain` for the archives). W_OK with `*out`;
 * W_ERR_NOT_FOUND with no resolver or no war3map.wpm in any place (a
 * build from before the converter copied it, or a map with none: no
 * pathing walls, not an error to the caller); W_ERR_NOT_FOUND with no
 * war3map.w3e either; the readers' codes, naming the file. */
WErr wc3_pathing_open(Resolver *r, MpqChain *chain, double scale, double x0, double y0, Wc3Pathing **out, WWhy *why);
void wc3_pathing_free(Wc3Pathing *p);

/* The cell under the converted world's (x, y): its bits, or -1 outside
 * the grid; its column and row in *i, *j (may be NULL). */
int wc3_pathing_flags(const Wc3Pathing *p, double x, double y, int *i, int *j);
/* 1 when the cell under (x, y) is land walkers may not enter. */
int wc3_pathing_blocks(const Wc3Pathing *p, double x, double y);

/* Whether a step from (x0, y0) to (x1, y1), her body `radius` round,
 * enters a cell that blocks from one that doesn't (one standing in a
 * closed cell may walk out of it); 1 with the wall's facing in *nx, *ny
 * (along the cells' side it crosses: the grid runs along the converted
 * world's axes), 0 with a NULL `p`. */
int wc3_pathing_wall(const Wc3Pathing *p, double x0, double y0, double x1, double y1, double radius, float *nx, float *ny);

/* Its size: cells across and up, and how many of them block. */
void wc3_pathing_size(const Wc3Pathing *p, int *width, int *height, int *blocking);

#endif
