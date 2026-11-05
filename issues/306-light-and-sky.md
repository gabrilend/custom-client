# 306 — Light.dbc, Sky, and Directional Light

*Filled in part by the code from wow-chat-2026 (its 173n), 2026-10-07.*

## Current behavior

Built (`src/026-light.c`, `src/037-sky.c`): Light.dbc's spheres by
place, blended, the default per map, the tables read through the
client's tables (074: rows across every place, placeholder rows, 097's
widths; 1109 step 1t); the colours and fog by time of day;
the sun's direction; the underwater set under water; the sky dome's five
colours as shader values; skybox models (Northrend's auroras). Not
built: weather and the ghost world's light (the sets are read the same
way), the original's clouds, sun and moon discs.

Measured against the clean-room checklist
([docs/057](../docs/057-what-the-original-program-decides.md) notes 4.1,
4.2 and 4.4, and section 4's fog and light rows, filled 2026-10-16):
- **Already the original's:** inches to yards (÷ 36); the outdoor
  position (17066.67 minus the record's third and first coordinates);
  time as 2880 half-minutes, blended in straight lines across midnight;
  colour band 18 × set − 17 + band and number band 6 × set − 5 + band;
  full strength inside the inner radius, a straight fade to the outer;
  fog end ÷ 36 and start as a share of the end.
- **Not yet the original's:**
  - ~~lights are laid **farthest first** by distance from the point (two
    within 1/3 yard by the smaller inner radius); this client sorts by
    outer radius;~~ Done in 1109 step 1t (decision 146): farthest first,
    a tie within 1/3 yard laid larger inner radius first, so the smaller
    is on top (that reading of the note is not checked against the
    original);
  - ~~after blending between two times, the original cuts each colour
    channel down to a whole step of 255; this client keeps the
    fraction;~~ Done in 1109 step 1u (decision 151): each colour band
    cut down to a whole 255th after the time blend, before lights and
    storms are blended;
  - ~~fog end is never below 10 yards nor beyond the far clip; the start
    multiplier is held to −1 to 1. This client sets a negative start to
    0 in every mode, while the original does that only in the second fog
    mode;~~ Done in 1109 step 1u (decisions 152, 153): on the blended
    light, the end floored at 10 yards, the share held to −1 to 1, a
    negative share kept below map 530 and 0 from it; the far clip as
    before (173b6);
  - ~~fog colour is sky colour 6;~~ Already so: the fog band, 7, is the
    sixth sky colour (bands 2 to 7; 1109 decision 155);
  - **the second fog mode** on maps numbered 530 and above when shaders
    are available: fog end pushed to the far clip when it's at least
    27.78 yards, a rate from 1.5 to 7 by how short the fog band is, the
    rate doubled under water; the curve itself is in the original's
    shipped shader files, which are data to read (note 4.2). The mode is
    chosen and its start rule kept (1109 step 1u, decision 153); the
    end's push, the rate and the curve are not built (no formula for the
    rate in these notes, the shader files not here);
  - indoors, outdoor fog blends into a building group's own fog over the
    first 25 yards past its boundary (its underwater fog in liquid);
  - ~~storm sets blended over the normal set by the storm's
    strength;~~ Done in 1109 step 1u (decision 154): `light_at_weather`,
    though nothing reads the weather yet (strength 0);
  - up to 5 zone lights by percentage; override lights with their own
    fades (not built: these notes name no table or fade rule; 1109
    decision 155);
  - ~~a map with no global light uses light record 1. This client instead
    falls back to fixed greys, a fog of 125 to 500 yards and water at
    0.75 when a light or its rows are missing, without saying so: three
    silent fallbacks, which the house rules count as warnings;~~ Done in
    1109 step 1t (decisions 145, 147, 150): a map with no default takes
    Light's row 1; the grey day stands only when there is no row 1 (said
    once) or the tables are in no place (said in one line); a missing
    band or parameter row is a placeholder row the resolver says, where
    it makes them;
  - **the sky dome** (note 4.4): seven rings of 24 points (single points
    at top and bottom) at 0°, 30.6°, 36°, 41.4°, 43.2°, 45° (the horizon,
    sky colour 6, the fog colour) and 180° from straight up, taking sky
    colours 1 to 6, on a sphere scaled by 6.67 and lowered by 0.707 of
    its radius; dawn and dusk tint by a day curve (peaks at 06:30 and
    21:30) times the light's "highlight sky", spread round the dome by a
    curve from the sun's direction (darker away from the sun); lightning
    and glow blends; a cloud dome (density 0.6); stars as
    `Environments\Stars\stars.mdl` when the star colour's alpha is at
    least 2. This client's dome has six rings at its own angles with
    five colours.

## Intended behavior

Three pieces, glued by a small lighting state struct.

### Light.dbc reader

```c
typedef struct {
    uint32_t map_id;
    vec3_t   center;
    float    radius_outer;
    float    radius_inner;
    uint32_t int_params[5];     /* indices into LightIntBand.dbc          */
    uint32_t float_params[5];   /* indices into LightFloatBand.dbc        */
} dbc_light_row_t;
```

`Light.dbc` rows describe **lighting zones**: a sphere on a map within
which a particular set of color/fog/sky parameters applies, fading from
inner radius (fully applied) to outer radius (zero contribution). The
parameters themselves are time-of-day curves stored in
`LightIntBand.dbc` and `LightFloatBand.dbc` (sun color, ambient color,
fog color, fog start/end, sky horizon/zenith colors, etc.).

For v1 we read the rows but **lock time-of-day to noon** — the curves
get sampled at one point. Day/night cycle is a Phase 10 polish concern.

### Lighting state

```c
typedef struct {
    vec3_t sun_direction;       /* world space, normalized */
    vec3_t sun_color;
    vec3_t ambient_color;
    vec3_t fog_color;
    float  fog_start;
    float  fog_end;
    vec3_t sky_horizon_color;
    vec3_t sky_zenith_color;
} lighting_t;

err_t lighting_init  (mpq_chain_t *, lighting_t **out);
void  lighting_free  (lighting_t *);

void  lighting_for_position(const lighting_t_db *, uint32_t map_id,
                            vec3_t world_pos, lighting_t *out);
```

Each frame the camera position picks a `lighting_t` via:

1. Start with the map's default row (any row whose center is the
   origin and radius is huge — every map has one).
2. For each `Light.dbc` row on the current map whose sphere contains
   the camera, blend its parameters in with weight derived from the
   inner→outer falloff.
3. Output the resulting `lighting_t`.

### Skybox

A small skybox subsystem that draws a cube with the inside facing
inward and shades each pixel by blending `sky_horizon_color` toward
`sky_zenith_color` based on view direction altitude. The sun disk is a
billboarded quad placed at `-sun_direction` (i.e., looking *toward* the
sun) with a soft-edged additive sprite.

```c
err_t skybox_init   (skybox_t **out);
void  skybox_close  (skybox_t *);
void  skybox_draw   (skybox_t *, const camera3d_t *, const lighting_t *);
```

Drawn **first** each frame with depth write off and depth test off,
filling the background; then the rest of the scene overwrites it.

### Hooking the renderers

`terrain_renderer_draw` and `wmo_renderer_draw` both take a
`const lighting_t *` parameter. Their shaders pick up
`u_sun_direction`, `u_sun_color`, `u_ambient_color`, and the four fog
uniforms.

## Suggested implementation steps

1. Extend [106](./106-dbc-parser.md)'s typed wrappers with
   `dbc_light_row_t`, `dbc_light_int_band_row_t`,
   `dbc_light_float_band_row_t`.
2. Create `src/lighting/` subsystem.
3. Implement `lighting_init`: read all three DBCs, build a per-map
   list of rows for fast lookup, sample the curves at the locked
   noon time.
4. Implement `lighting_for_position`: walk the map's rows, accumulate
   weighted blends. Default row (radius_inner = 0, radius_outer huge)
   provides the base.
5. Create `src/skybox/` subsystem with the cube + sun-disk renderer.
6. Update `terrain.frag` and `wmo_outdoor.frag` to consume the new
   lighting uniforms. Implement the height-fog blend in clip-space at
   the end of the fragment shader (exponential, as documented in
   [docs/005](../docs/005-rendering-pipeline.md)).
7. Tests: standing at Northshire's spawn point produces a
   `lighting_t` whose sun color matches the well-known "Elwynn green"
   tint (eyeball + reference screenshot).
8. The original's light, fog and sky (docs/057 notes 4.1, 4.2, 4.4; link
   back when done), each difference listed under Current behavior:
   lights laid farthest first; colours cut to whole steps; the fog end's
   floor and cap and the start's range; the two fog modes by map number,
   the second mode's rate and its doubling under water, the curve read
   from the original's shipped shader files rather than step 6's
   exponential guess (on maps below 530 the fog is plain start to end);
   the indoor fog blend; storms, zone lights and overrides; light
   record 1 for a map without a global light, and the three silent
   fallbacks replaced by named errors; the seven-ring dome with its
   dawn and dusk tint, clouds and stars.
9. Tests: two overlapping lights at 10 and 40 yards from the point lay
   the nearer on top; a fog end stored as 180 inches becomes 10 yards
   (the floor), not 5; on map 0 a negative start multiplier is kept,
   and on map 571 it counts as 0; at 06:30 the ring facing the sun
   carries the full tint and the ring opposite the darker colour.

## Acceptance

- Standing at Northshire, the terrain and chapel are lit with the
  characteristic Elwynn warm greenish tone, not the placeholder
  white-on-grey of pre-306.
- Flying high above the world, the sky fades smoothly from horizon to
  zenith color.
- Fog at distance behaves like the original client (you can see
  roughly the same number of tiles away).

## Stress markers

The DBC band-curve format (LightIntBand / LightFloatBand) is denormalized
in an annoying way: each row has up to 16 (time, value) keyframes
stored as parallel arrays of fixed length. Trailing slots are
zero-padded; the count says how many are real. Reading "all 16" yields
junk colors near midnight.

The blending weight between overlapping Light.dbc zones isn't documented
super precisely. Use `smoothstep(radius_outer, radius_inner, distance)`
as the per-zone weight, then normalize across all overlapping zones.
This matches what most of the open-source clients converged on; if it
looks wrong against reference screenshots, that's the first place to
poke.

## Related

- [docs/003-asset-formats.md](../docs/003-asset-formats.md#dbc--client-side-data-tables)
- Blocked by: [106](./106-dbc-parser.md), [302](./302-terrain-renderer.md),
  [305](./305-wmo-renderer.md), [207](./207-camera.md).
- Blocks: [308](./308-phase-demo.md).
- **Blocks:** [1002](./1002-zone-music-and-ambience.md).
- Follow-up: day/night cycle (Phase 10).
