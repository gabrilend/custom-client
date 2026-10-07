# 038-ground

*Written by `scripts/generate-info` from `038-ground.h`; don't edit, re-run it.*

038-ground.h (issue 173f) - the painted ground

For a general audience: draws the ground as the original client paints
it. For each map tile near her it reads the tile's ground file (020-adt.h),
makes one small mesh per 33-yard chunk from its heights, loads the
chunk's ground pictures (each loaded once and shared), and packs the
chunk's blend maps into one small picture: red, green and blue say how
strongly layers 1, 2 and 3 show, alpha holds the baked shadow (for
173h). A shader then blends the pictures point by point, lights them by
the zone's sun and shade, and fades them into its fog (173n).

Drawing needs Raylib and a window; reading (020-adt.c) and preparing a tile
(033-ground-prep.h: its points, blend picture and pictures, the arithmetic
half, which the reading-ahead thread does ahead of her) don't, and are
what the tests check. This file is the sending half.

## What it offers

- `typedef struct Ground Ground;`
  Readies the painter for one map, at `place` (ground_place_init: the
  archives, the map, its blend-map form, the server's heights, the
  drawing origin); what it points to must outlive the painter. W_OK, or a code
  with `why`.
- `Ground *ground_new(const GroundPlace *place, WWhy *why);`
- `void ground_free(Ground *g);`
- `int ground_on_card(void *ground, const char *name);`
  Whether a ground picture (by archive name) is on the card now. Safe on
  any thread: given to ground_prep_tile so pictures already sent aren't
  unpacked again.
- `WErr ground_load_tile(Ground *g, GroundPrep *prepared, AdtTile **kept, WWhy *why);`
  Sends a prepared tile (ground_prep_tile, on either thread) to the card
  and draws it from then on; the preparation is used up (freed). The
  ground file read is handed back in `*kept` (for the water, doodads and
  buildings on it); the caller frees it with adt_free and free.
  W_OK, or a code with `why` (a picture couldn't be had). Main thread only.
- `void ground_drop_tile(Ground *g, int tx, int ty);`
  Drops tile (tx, ty): its meshes and blend pictures (its ground pictures
  stay loaded, shared and cached; 173s).
- `void ground_blob(Ground *g, float x, float y, float z, float radius);`
  A soft round shadow on the ground under a character this frame:
  centre (relative to the drawing origin) and radius in yards. Up to 8
  a frame; the list empties after each ground_draw.
- `void ground_draw(Ground *g, Vector3 eye, const LightNow *light);`
  Draws every loaded tile. `eye` is the camera's position (for the fog).
- `int ground_orders_last(Ground *g);`
  How many draw orders the ground sent since last asked (for the picture
  report; 173u).
