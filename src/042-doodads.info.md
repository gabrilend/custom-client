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

How a placement becomes a position and a turn is adt_place_matrix
(020-adt.h), shared with the buildings.

## What it offers

- `int doodads_init(MpqChain *chain, CollideWorld *collide, LiquidWorld *liquids, char *why, int why_len);`
  Readies the drawer; the chain must outlive it. Every copy placed also
  adds its model's solid shape to `collide` (173q; NULL: none), and every
  building placed its pools to `liquids` and to the water drawer (173r;
  NULL: none). 0, or 1 with `why`.
- `int doodads_add_tile(const AdtTile *t, int tile, double ox, double oy, char *why, int why_len);`
  Adds a tile's doodads, placed relative to the drawing origin (ox, oy),
  loading any model not seen before. A model that can't be read is
  reported once by name and left out; 0, or -1 with `why` for a fault
  that stops the client (the graphics card refusing a shader).
- `void doodads_drop_tile(int tile);`
  Drops tile `tile` (tx * 64 + ty): its copies, their solid triangles,
  its placements; doodads it shared with a tile still loaded stay (173s).
- `void doodads_draw(Vector3 eye, const LightNow *light);`
  Draws every doodad. `eye` is the camera's position (for the fog).
- `void doodads_animate(uint32_t ms, float dt, Vector3 eye, Vector3 target, float particle_density);`
  Bends every moving model (one whose standing animation moves: birds,
  banners) to its pose at `ms` since start; all copies share it. Runs the
  particles of every copy within 150 yards of the camera (`eye`, looking
  at `target`) for `dt` seconds (173o), each emitter's birth rate times
  `particle_density` (0.1 to 1: the Particle Density setting, 173b6).
- `void doodads_skybox(const char *path);`
  The skybox (173n): the model at `path` (an M2; "" or NULL: none) is
  drawn round the camera, behind everything, unlit and unfogged, sized to
  800 yards, posed by its stand animation and tinted and layered as any
  doodad. doodads_draw_skybox draws it (call it first).
- `void doodads_draw_skybox(Vector3 eye);`
- `void doodads_counts(int *models, int *copies, int *skipped_pieces, int *moving, int *emitting);`
  How many models and copies are loaded, how many pieces were left out
  because their picture is chosen by the game at run time, how many
  models move, and how many have particles (for the start-up report).
- `int doodads_orders_last(void);`
  How many draw orders the doodads sent since last asked (for the
  picture report; 173u). Only copies within the camera's sight are drawn.
