/*
 * 084-map-fog.h (issue 1109, step 1j) - a converted map's own fog, from
 * its war3map.w3i, in place of the zone's
 *
 * For a general audience: a Warcraft III map can set the haze its world
 * fades into with distance: where it starts, where everything is lost in
 * it, and its colour (the editor's "terrain fog"). In a test-map run this
 * client draws that fog instead of the one the game's zone tables give
 * (026), over the ground, the water, the doodads and the figures alike,
 * since all of them take their fog from the one LightNow each frame. A
 * map that turns no fog on, has no war3map.w3i in its build, or is a
 * Reign of Chaos map (no fog in its file) keeps the zone's fog, as
 * before.
 *
 * The rule (issue 1109, decisions 76 to 80):
 *   on when the file has its environment (version 25 and later) and the
 *     editor's "use terrain fog" flag is set (W3I_FLAG_TERRAIN_FOG);
 *   start and end are the file's distances from the eye, in WC3 units,
 *     times the map's yards per unit (map.lua's rule, 073): the start no
 *     closer than 0, the end no closer than the start;
 *   the colour is the file's red, green and blue over 255; its alpha and
 *     the density are not used;
 *   every style is drawn linear from the start to the end, as each
 *     shader here fogs (the exponential styles are a look not matched).
 * Underwater, the zone's underwater light is kept (the caller doesn't
 * apply the map's fog there), and the view distance still ends the fog
 * at the latest (map_fog_frame clamps after applying).
 *
 * Window-free (CORE: the test program and the executable).
 */
#ifndef EG_MAP_FOG_H
#define EG_MAP_FOG_H
#include "013-errors.h"
#include "015-mpq.h"
#include "026-light.h"
#include "060-resolver.h"
#include "083-w3i.h"

#define MAP_W3I_NAME "war3map.w3i"   /* at the build folder's root, where the converter copies it */

/* The map's fog, field by field:
 *   on          1 when it replaces the zone's fog
 *   style       the file's (0 linear, 1 exponential, 2 exponential
 *               squared), kept to be said; drawn linear whatever it is
 *   start, end  yards from the eye
 *   rgb         red, green, blue, 0 to 1
 *   why_off     when on is 0, the reason in a few words (static text) */
typedef struct {
    int         on;
    int         style;
    float       start, end;
    float       rgb[3];
    const char *why_off;
} MapFog;

/* Reads the map's war3map.w3i through `r` (a test-map run's resolver:
 * its build folder first) into `out`. W_OK; W_ERR_NOT_FOUND, with why,
 * when there is no resolver or no place has the file; 083's refusals
 * (W_ERR_CORRUPT, W_ERR_UNSUPPORTED) and the resolver's, naming the
 * file. */
WErr map_w3i_read(Resolver *r, MpqChain *chain, W3iInfo *out, WWhy *why);

/* The map's fog from its settings `in` (NULL: no war3map.w3i) at
 * `yards_per_unit` (map.lua's wc3_scale). Off, with why_off, for no
 * file, a Reign of Chaos file, the flag unset, a scale not above 0, or
 * a start or end that isn't a number. */
void map_fog_from_w3i(const W3iInfo *in, double yards_per_unit, MapFog *out);

/* Puts the map's fog into `l` (its colour, start and end; the rest of
 * the light stays the zone's) and returns 1; returns 0 and leaves `l`
 * as it is when `f` is NULL or off. */
int map_fog_apply(const MapFog *f, LightNow *l);

/* A frame's fog, as 043 sets it after reading the zone's light into
 * `l`: the map's fog (`f`, NULL or off for none) unless `under` (the eye
 * below the water: the zone's underwater light stays), then the fog's
 * end brought in to the view distance `farclip` when past it, its start
 * by the same share (173b6). Returns 1 when the map's fog is used. */
int map_fog_frame(const MapFog *f, int under, float farclip, LightNow *l);

#endif
