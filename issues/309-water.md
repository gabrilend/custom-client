# 309 — Water

*Came from: wow-chat-2026 173r (water and swimming; the swimming half
continues in 801), moved 2026-10-07.*

## Current behavior

Built:
- **Reading:** the ground files' liquids (MH2O: per chunk, its kind, its
  height grid, which cells it covers; `src/020-adt.c`) and buildings'
  pools (MLIQ; `src/021-wmo.c`), placed as their building is.
- **Where the surface is** (`src/027-liquid.c`, window-free): the water's
  height anywhere, for the mover and the camera.
- **Drawing** (`src/040-water.c`, a handle): each layer with its liquid's
  own scrolling pictures (LiquidType.dbc), coloured and made see-through
  by the zone's water colours and opacities (the light's river and ocean
  bands), sight-tested; magma and slime drawn opaque.
- **Under water:** with the camera below the surface, the light is the
  zone's own underwater set (`026-light.c`).
- Tests: Astranaar's lake has water and the town none; dropping a tile
  forgets its water.

## Intended behavior

Water looks and behaves as the original's: the ground's and buildings'
liquids drawn with their pictures and the zone's colours, the view under
water lit as the zone says, and the surface known to everything that
moves.

## Suggested implementation steps

1. Compare a lake, the sea and a building's pool with the original side
   by side.
2. Reflections, if the original's quality setting asks for them.

## Related

- [801](./801-movement-by-the-servers-rules.md) (swimming), [306](./306-light-and-sky.md).
