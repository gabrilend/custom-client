# 040-water

*Written by `scripts/generate-info` from `040-water.h`; don't edit, re-run it.*

040-water.h (issue 173r) - drawing the water

For a general audience: each ground file's water layers (020-adt.h) become
flat-ish meshes over their wet cells, at the surface heights the file
gives. Each kind of water (LiquidType.dbc) has a numbered run of
pictures (lake_a.1.blp, lake_a.2.blp ...) shown one after another so
the surface ripples. Water and the sea are tinted by the zone's water
colours (026-light.h: river and ocean, near and deep, by time of day) and
drawn see-through over the ground, after the solid world; lava and
slime are drawn as their pictures, unlit and solid.

## What it offers

- `int water_init(MpqChain *chain, char *why, int why_len);`
  Reads the liquid table; the chain must outlive the drawer. 0, or 1
  with `why`.
- `void water_add_tile(const AdtTile *t, int tile, double ox, double oy);`
  Adds a tile's water (tile = tx * 64 + ty), placed relative to the
  drawing origin; drops it again (173s).
- `void water_drop_tile(int tile);`
- `void water_add_pool(int tile, const float at[16], const WmoPool *p, int kind);`
  Adds a building's pool (021-wmo.h), of kind `kind` (LiquidType.dbc id),
  placed by `at` (the building's placement, Raylib's layout, relative to
  the drawing origin), brought by `tile`; water_drop_tile drops it. Its
  wet cells are drawn as the ground's water is, the picture repeating
  once a cell.
- `void water_draw(Vector3 eye, const LightNow *light, double seconds);`
  Draws all water at `seconds` since start (the ripples' clock).
