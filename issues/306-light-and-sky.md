# 306 — Light.dbc, Sky, and Directional Light

*Filled in part by the code from wow-chat-2026 (its 173n), 2026-10-07.*

## Current behavior

Built (`src/026-light.c`, `src/037-sky.c`): Light.dbc's spheres by
place, blended, the default per map; the colours and fog by time of day;
the sun's direction; the underwater set under water; the sky dome's five
colours as shader values; skybox models (Northrend's auroras). Not
built: weather and the ghost world's light (the sets are read the same
way), the original's clouds, sun and moon discs.

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
- Follow-up: day/night cycle (Phase 10).
