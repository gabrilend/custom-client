# 042-doodads

*Written by `scripts/generate-info` from `042-doodads.h`; don't edit, re-run it.*

042-doodads.h (issue 173g) - trees, rocks and other small models on the map

For a general audience: every ground file lists the small models placed
on its tile (trees, rocks, fences, lamps: "doodads"), each with a
position, a turn and a size. This loads each model once (its points,
pieces and pictures, through 022-m2.c and 017-blp.c) and draws all its copies
in one go per piece (instancing), standing in their resting pose.
A doodad standing on two tiles is listed in both with one unique id,
and drawn once.

Models, buildings and their pictures come through the asset resolver
when one is given (issue 1109's step 1c; 076-placed-models.h): a map
converted from Warcraft III places its own WC3 models (061-mdx.h),
found in its folder, drawn standing still in their rest pose and sized
by the map's yards per WC3 unit; a model or building no place has is
the placeholder box, drawn as a magenta wire-frame (a building's as
big as its box); a picture no place has, the magenta checker. With
no resolver (every run but a test map's) the archives alone, as
before: what they lack is reported and left out.

How a placement becomes a position and a turn is adt_place_matrix
(020-adt.h), shared with the buildings.

## What it offers

- `typedef struct Doodads Doodads;`
  Readies the drawer; the chain and the resolver (`assets`, NULL: the
  archives alone) must outlive it. Every copy placed also
  adds its model's solid shape to `collide` (173q; NULL: none), and every
  building placed its pools to `liquids` and to the water drawer (173r;
  NULL: none). W_OK, or a code with `why`.
- `Doodads *doodads_new(MpqChain *chain, Resolver *assets, Textures *textures, CollideWorld *collide, LiquidWorld *liquids, Water *water, WWhy *why);`
- `void doodads_wc3_scale(Doodads *dd, double yards_per_unit);`
  A converted map's yards per WC3 unit (map.lua's wc3_to_wow.scale,
  073-converted-map.h), for its own WC3 models' points; 1/30.72 until
  set. Set it before the first tile is added.
- `void doodads_sources(Doodads *dd, int *wc3_models, int *stand_ins);`
  How many of the models loaded are WC3 models a map brought, and how
  many the placeholder box stood in for (for the start-up report).
- `void doodads_free(Doodads *dd);`
- `WErr doodads_add_tile(Doodads *dd, const AdtTile *t, int tile, double ox, double oy, WWhy *why);`
  Adds a tile's doodads, placed relative to the drawing origin (ox, oy),
  loading any model not seen before. A model that can't be read is
  reported once by name and left out; W_OK, or a code with `why` for a fault
  that stops the client (the graphics card refusing a shader).
- `void doodads_drop_tile(Doodads *dd, int tile);`
  Drops tile `tile` (tx * 64 + ty): its copies, their solid triangles,
  its placements; doodads it shared with a tile still loaded stay (173s).
- `void doodads_draw(Doodads *dd, Vector3 eye, const LightNow *light);`
  Draws every doodad. `eye` is the camera's position (for the fog).
- `void doodads_animate(Doodads *dd, uint32_t ms, float dt, Vector3 eye, Vector3 target, float particle_density);`
  Bends every moving model (one whose standing animation moves: birds,
  banners) to its pose at `ms` since start; all copies share it. Runs the
  particles of every copy within 150 yards of the camera (`eye`, looking
  at `target`) for `dt` seconds (173o), each emitter's birth rate times
  `particle_density` (0.1 to 1: the Particle Density setting, 173b6).
- `void doodads_skybox(Doodads *dd, const char *path);`
  The skybox (173n): the model at `path` (an M2; "" or NULL: none) is
  drawn round the camera, behind everything, unlit and unfogged, sized to
  800 yards, posed by its stand animation and tinted and layered as any
  doodad. doodads_draw_skybox draws it (call it first).
- `void doodads_draw_skybox(Doodads *dd, Vector3 eye);`
- `void doodads_counts(Doodads *dd, int *models, int *copies, int *skipped_pieces, int *moving, int *emitting);`
  How many models and copies are loaded, how many pieces were left out
  because their picture is chosen by the game at run time, how many
  models move, and how many have particles (for the start-up report).
- `int doodads_orders_last(Doodads *dd);`
  How many draw orders the doodads sent since last asked (for the
  picture report; 173u). Only copies within the camera's sight are drawn.
