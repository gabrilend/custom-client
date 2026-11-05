/*
 * 026-light.h (issue 173n) - the zone's light, by place and time of day
 *
 * For a general audience: the original lights each part of the world by
 * its own tables. Light.dbc places light spheres on each map (Ashenvale
 * has its own, as does Astranaar's valley), plus one default per map for
 * anywhere no sphere reaches. Each light names a parameter set whose
 * colours (LightIntBand.dbc: the sun's light, the shade's light, five sky
 * colours from overhead to the horizon, the fog's colour, and more) and
 * distances (LightFloatBand.dbc: how far the fog ends, and where it
 * starts) each change through the day: a list of up to 16 (time, value)
 * pairs, blended between. This reads those tables and answers "what is
 * the light here, now?".
 *
 * Layouts (3.3.5a):
 *   Light: 1 id, 2 map, 3-5 position (x, y, z, all times 36, in
 *     placement space: world x = 17066.67 - z/36, world y = 17066.67 -
 *     x/36, height y/36), 6 inner radius, 7 outer radius (times 36), 8-12
 *     parameter sets: 8 clear weather, 9 clear underwater, 10 storm, 11
 *     storm underwater, 12 death (the ghost world); 13-15 unused (0 on
 *     every row). A light at position 0, 0, 0 is the map's default.
 *   LightIntBand: 1 id (= set * 18 - 17 + band), 2 count, 3-18 times
 *     (half-minutes since midnight, 0-2880), 19-34 colours (0xRRGGBB).
 *     Bands used: 0 sun, 1 shade (ambient), 2-6 sky overhead to horizon,
 *     7 fog, 14-15 ocean near and deep, 16-17 river near and deep (173r).
 *   LightParams (9 fields): 1 id, 2 highlight sky, 3 skybox (LightSkybox
 *     id), 4 cloud kind, 5 glow, 6-7 the river's opacity shallow and deep,
 *     8-9 the ocean's (floats; Ashenvale's: 0.5, 1, 0.75, 1).
 *   LightFloatBand: 1 id (= set * 6 - 5 + band), 2 count, 3-18 times,
 *     19-34 values. Band 0: where the fog ends (times 36), band 1: where it
 *     starts, as a share of that.
 *
 * Inside a sphere's inner radius its light is used whole; between inner
 * and outer it blends into whatever lies beyond, by distance: the
 * default first, then the spheres reaching here farthest first (their
 * centres from here; two within a third of a yard of each other, the
 * larger inner radius first), so the nearest is laid on top (306's
 * notes on the original; 1109 step 1t, decision 146). A map with no
 * default of its own takes Light's row 1 (306's notes: the original's),
 * and with no row 1 either, the plain grey day, said once at light_open (decision 147).
 *
 * Read through the client's tables (074, 1109 step 1t): a converted
 * map's own Light rows are met before the archives', and a band, set or
 * skybox row no place has is a placeholder row in a test-map run. With
 * any of the four tables in no place, the plain grey day everywhere,
 * said once.
 *
 * A LightNow, field by field (colours are red, green, blue, 0 to 1):
 *   sun[3], shade[3]     the sun's light and the light in shadow
 *   sky[5][3]            the sky's colours, overhead first, horizon last
 *   fog[3]               the fog's colour
 *   water[4][3]          ocean near, ocean deep, river near, river deep
 *   fog_start, fog_end   float, yards
 *   water_alpha[4]       the water's opacity, 0-1, in water[]'s order:
 *                        ocean shallow, ocean deep, river shallow, river
 *                        deep (LightParams 8, 9, 6, 7; 173r)
 *   sun_dir[3]           a unit vector toward the sun (world axes)
 *   skybox[128]          the skybox model the strongest light names
 *                        (LightParams 3 -> LightSkybox 2), "" for none
 */
#ifndef EG_LIGHT_H
#define EG_LIGHT_H

#include "013-errors.h"

#include "016-dbc.h"
#include "060-resolver.h"
#include "074-tables.h"

#include <stdint.h>
#include <stdio.h>

typedef struct {
    float sun[3], shade[3];
    float sky[5][3];
    float fog[3];
    float water[4][3];
    float fog_start, fog_end;
    float water_alpha[4];             /* opacity: ocean shallow, ocean deep, river shallow, river deep (LightParams 8, 9, 6, 7) */
    float sun_dir[3];
    char  skybox[128];
} LightNow;

/* One of Light's rows, read once: its id and map, where it stands in
 * world axes (yards), its inner and outer radius (yards), and its five
 * parameter sets (Light's fields 8-12). */
typedef struct {
    int      id, map;
    float    x, y, z;
    float    inner, outer;
    uint32_t set[5];
    int      fallback;                /* 1: at 0, 0, 0, the map's default light */
} LightSphere;

/* The light tables, read through the client's tables (074): Light's rows
 * from every place that has it, a map's own first (a search: a converted
 * map's lights beside the archives'), the bands, parameter sets and
 * skyboxes looked up by number as each light needs them, a row no place
 * has a placeholder row where the tables have a resolver (074: said and
 * counted; strict mode refuses it), its band then the plain grey day's.
 * `none` 1: Light, LightParams, LightIntBand or LightFloatBand is in no
 * place, so every place has the plain grey day (said once, at open). */
typedef struct {
    Tables      *tables;              /* not owned: the start's (087's ws.tables, 043's) */
    LightSphere *lights;
    int          nlights;
    int          none;
    int          ncopies;             /* Light's copies (074), 0 with none */
    AssetSource  from;                /* where Light's first copy came from (060) */
    int          skyboxes;            /* 1: LightSkybox is in some place */
    int          record1;             /* the index of Light's row 1 (any map's), -1 none: a map with no default of its own takes it */
} LightTables;

/* Reads Light's rows through `tables` and checks the other four are in
 * some place. W_OK, saying on `log` (may be NULL) when one is missing:
 * then `none` is 1 and light_at gives the plain grey day. A table that is
 * there but can't be read (074: not a table, narrower than the columns
 * read here, copies of different widths) is its error, naming it. */
WErr  light_open(LightTables *t, Tables *tables, FILE *log, WWhy *why);
void light_close(LightTables *t);

/* The plain grey day: a map with no lights, or no light tables at all
 * (the fixed sky and fog this client draws without them); also each
 * band of a light whose row no place has. */
#define LIGHT_GREY_SUN    0.8f
#define LIGHT_GREY_SHADE  0.4f
#define LIGHT_GREY_SKY    0.6f
#define LIGHT_GREY_FOG    0.6f
#define LIGHT_GREY_FOG_END   500.0f   /* yards */
#define LIGHT_GREY_FOG_SHARE 0.25f    /* where it starts, as a share of its end */

/* Which of a light's parameter sets is used (Light's fields 8 on). */
enum { LIGHT_CLEAR = 0, LIGHT_UNDERWATER = 1 };

/* The light at world (x, y, z) on map `map` at `hours` since midnight
 * (0-24), of kind `kind` (LIGHT_CLEAR, or LIGHT_UNDERWATER while the
 * camera is below the water's surface: 173n). Returns the id of the light whose colours weigh most (for
 * reports), or -1 when the map has no lights at all (then `out` holds a
 * plain grey day, and the caller is told). */
int  light_at(const LightTables *t, int map, float x, float y, float z, float hours, int kind, LightNow *out);

#endif
