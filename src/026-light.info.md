# 026-light

*Written by `scripts/generate-info` from `026-light.h`; don't edit, re-run it.*

026-light.h (issue 173n) - the zone's light, by place and time of day

For a general audience: the original lights each part of the world by
its own tables. Light.dbc places light spheres on each map (Ashenvale
has its own, as does Astranaar's valley), plus one default per map for
anywhere no sphere reaches. Each light names a parameter set whose
colours (LightIntBand.dbc: the sun's light, the shade's light, five sky
colours from overhead to the horizon, the fog's colour, and more) and
distances (LightFloatBand.dbc: how far the fog ends, and where it
starts) each change through the day: a list of up to 16 (time, value)
pairs, blended between. This reads those tables and answers "what is
the light here, now?".

Layouts (3.3.5a):
  Light: 1 id, 2 map, 3-5 position (x, y, z, all times 36, in
    placement space: world x = 17066.67 - z/36, world y = 17066.67 -
    x/36, height y/36), 6 inner radius, 7 outer radius (times 36), 8-12
    parameter sets: 8 clear weather, 9 clear underwater, 10 storm, 11
    storm underwater, 12 death (the ghost world); 13-15 unused (0 on
    every row). A light at position 0, 0, 0 is the map's default.
  LightIntBand: 1 id (= set * 18 - 17 + band), 2 count, 3-18 times
    (half-minutes since midnight, 0-2880), 19-34 colours (0xRRGGBB).
    Bands used: 0 sun, 1 shade (ambient), 2-6 sky overhead to horizon,
    7 fog, 14-15 ocean near and deep, 16-17 river near and deep (173r).
  LightParams (9 fields): 1 id, 2 highlight sky, 3 skybox (LightSkybox
    id), 4 cloud kind, 5 glow, 6-7 the river's opacity shallow and deep,
    8-9 the ocean's (floats; Ashenvale's: 0.5, 1, 0.75, 1).
  LightFloatBand: 1 id (= set * 6 - 5 + band), 2 count, 3-18 times,
    19-34 values. Band 0: where the fog ends (times 36), band 1: where it
    starts, as a share of that.

Inside a sphere's inner radius its light is used whole; between inner
and outer it blends into whatever lies beyond, by distance: the
default first, then the spheres reaching here farthest first (their
centres from here; two within a third of a yard of each other, the
larger inner radius first), so the nearest is laid on top (306's
notes on the original; 1109 step 1t, decision 146). A map with no
default of its own takes Light's row 1 (306's notes: the original's),
and with no row 1 either, the plain grey day (decision 147), said by
light_map_default for the map being lit (decision 164).

Read through the client's tables (074, 1109 step 1t): a converted
map's own Light rows are met before the archives', and a band, set or
skybox row no place has is a placeholder row in a test-map run. With
any of the four tables in no place, the plain grey day everywhere,
said once.

A LightNow, field by field (colours are red, green, blue, 0 to 1; a light's own in whole 255ths before lights are blended):
  sun[3], shade[3]     the sun's light and the light in shadow
  sky[5][3]            the sky's colours, overhead first, horizon last
  fog[3]               the fog's colour
  water[4][3]          ocean near, ocean deep, river near, river deep
  fog_start, fog_end   float, yards
  water_alpha[4]       the water's opacity, 0-1, in water[]'s order:
                       ocean shallow, ocean deep, river shallow, river
                       deep (LightParams 8, 9, 6, 7; 173r)
  sun_dir[3]           a unit vector toward the sun (world axes)
  fog_mode             the original's fog mode the light's fog follows:
                       1 below map LIGHT_FOG_MODE2_MAP, 2 from it on,
                       0 for the grey day (this client's own fog)
  skybox[128]          the skybox model the strongest light names
                       (LightParams 3 -> LightSkybox 2), "" for none

## What it offers

- `type LightNow`
- `type LightSphere`
  One of Light's rows, read once: its id and map, where it stands in
  world axes (yards), its inner and outer radius (yards), and its five
  parameter sets (Light's fields 8-12).
- `type LightTables`
  The light tables, read through the client's tables (074): Light's rows
  from every place that has it, a map's own first (a search: a converted
  map's lights beside the archives'), the bands, parameter sets and
  skyboxes looked up by number as each light needs them, a row no place
  has a placeholder row where the tables have a resolver (074: said and
  counted; strict mode refuses it), its band then the plain grey day's.
  `none` 1: Light, LightParams, LightIntBand or LightFloatBand is in no
  place, so every place has the plain grey day (said once, at open).
- `WErr light_open(LightTables *t, Tables *tables, FILE *log, WWhy *why);`
  Reads Light's rows through `tables` and checks the other four are in
  some place. W_OK, saying on `log` (may be NULL) when one is missing:
  then `none` is 1 and light_at gives the plain grey day. A table that is
  there but can't be read (074: not a table, narrower than the columns
  read here, copies of different widths) is its error, naming it.
- `void light_close(LightTables *t);`
- `int light_map_default(const LightTables *t, int map, FILE *log);`
  The light under every sphere on map `map`: its own default's id, else
  Light's row 1's, else -1 (the grey day where no sphere reaches), which
  is said on `log` (may be NULL), once per call: 043 and 089 call it once
  for the map they light (1109 decision 164, so a converted map with a
  default light of its own isn't told about row 1).
- `int light_at(const LightTables *t, int map, float x, float y, float z, float hours, int kind, LightNow *out);`
  The light at world (x, y, z) on map `map` at `hours` since midnight
  (0-24), of kind `kind` (LIGHT_CLEAR, or LIGHT_UNDERWATER while the
  camera is below the water's surface: 173n). Returns the id of the light whose colours weigh most (for
  reports), or -1 when the map has no lights at all (then `out` holds a
  plain grey day, and the caller is told).
- `int light_at_weather(const LightTables *t, int map, float x, float y, float z, float hours, int kind, float storm, LightNow *out);`
  light_at in a storm of strength `storm`, 0 (none: light_at's) to 1:
  each light's storm set (Light's field 10, under water 11) laid over its
  set by that share, where it names one (306's notes: the original's;
  1109 decision 154). 043 and 089 pass 099's eased strength (the
  weather from --weather or a moves file until there is network code;
  1109 step 1v).
- `void light_indoor_fog(LightNow *l, const WmoFog *f, float depth, int under);`
  Indoors (306's notes: the original's): the outdoor fog in `l` blends
  into a building group's own fog `f` over the first
  LIGHT_INDOOR_FOG_YARDS past the group's boundary, `depth` yards in
  (021's wmo_group_depth; 0 or less: outside, `l` as it is), its
  underwater fog when `under`. Its colour, start and end; straight-line
  over the 25 yards (decision 162). Which of a group's fogs, and whether
  the point is in the group at all, are the caller's: neither is built
  yet (decision 163), so nothing calls this but the tests.
