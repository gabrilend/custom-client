/*
 * 037-sky.h (issue 173n) - the sky dome
 *
 * For a general audience: a big upturned bowl around the camera, painted
 * with the zone's five sky colours (026-light.h): the horizon's at the rim,
 * then two bands, then the middle, then overhead at the top, blended
 * between. It is drawn first, behind everything, and moves with the
 * camera so it can never be reached.
 *
 * A skybox (173n): when the zone's light names one (LightSkybox: in
 * Kalimdor and the Eastern Kingdoms none do; in Northrend the auroras),
 * the doodad drawer draws its model round the camera after the dome
 * (doodads_skybox): behind everything, sized to 800 yards, posed,
 * layered and tinted as any doodad, unlit.
 *
 * Rings, by height above the horizon (degrees): -30 and 0 the horizon
 * colour (sky[4]), 6 sky[3], 15 sky[2], 35 sky[1], 90 overhead (sky[0]).
 * This client's reading of where the original's bands sit, to be
 * compared with it.
 */
#ifndef EG_SKY_H
#define EG_SKY_H

#include "026-light.h"
#include "raylib.h"

/* A sky (the handle: docs/001, "Handles"): the dome made and sent once.
 * Needs the window. Freed with sky_free. */
typedef struct Sky Sky;
Sky *sky_new(void);
void sky_free(Sky *sk);

/* Hands a shader the light: its uniforms sun_dir, sun_col, shade_col,
 * fog_col (vec3) and fog_range (vec2: where the fog starts and ends,
 * yards). Every world shader (ground, doodads, figures) declares them. */
void sky_light_shader(Shader sh, const LightNow *light);
void sky_draw(Sky *sk, Vector3 eye, const LightNow *light);

#endif
