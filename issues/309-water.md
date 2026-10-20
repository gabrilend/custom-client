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

Measured against the clean-room checklist's note 4.6
([docs/057](../docs/057-what-the-original-program-decides.md), filled
2026-10-16), where this differs from the original:
- **The flipbook:** frames are loaded here until one is missing (up to
  32) and shown at 15 a second. The original loads frames 1 to 30 and
  shows the frame at the current share of a loop period: the liquid
  record's own period for the first texture, always 1250 ms for the
  second, on the program's millisecond clock, so all water of one kind
  ripples in step. Water isn't drawn until every frame has loaded.
- **Placing the textures:** the liquid record's first number scales the
  first texture, its second turns it (radians), its third scales the
  second texture; not used here.
- **Colour and see-through amount:** both from 64-step ramps rebuilt as
  the light changes: the ocean from water colour 1 to 2 (the deepest
  step at 90% brightness), inland from 3 to 4, each with its shallow to
  deep alpha, each step cut down to whole numbers; inside buildings,
  water colour 4 beside a white strip. Here the tint and opacity blend
  by depth in the shader, without the ramps; how deep each step lies is
  in the original's shipped shader files.
- **Drawing:** alpha blended, both faces, fogged, depth written only on
  devices without user clip planes, up to three nearby lights.
- **Procedural textures:** names containing `procedural` are built by the
  program; a missing one shows solid green in the original (a
  **quirk**, with a correction switch off by default that leaves the
  water undrawn and says so).

## Intended behavior

Water looks and behaves as the original's: the ground's and buildings'
liquids drawn with their pictures and the zone's colours, the view under
water lit as the zone says, and the surface known to everything that
moves.

## Suggested implementation steps

1. Compare a lake, the sea and a building's pool with the original side
   by side.
2. Reflections, if the original's quality setting asks for them.
3. The original's water (docs/057 note 4.6, above; link back when
   done): the 30-frame flipbook timed by each texture's period, texture
   scale and turn, the two depth ramps from the light, the building
   ramp, the procedural-texture quirk and its switch. Tests: two lakes
   of the same kind show the same frame at the same moment; with a
   period of 3000 ms, frame 16 shows at 1.5 s; the ocean ramp's deepest
   step is 90% as bright as water colour 2.

## Related

- [801](./801-movement-by-the-servers-rules.md) (swimming), [306](./306-light-and-sky.md).
