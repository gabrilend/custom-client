# 310 — Shadows and Live Light

*Came from: wow-chat-2026 173h (shadow maps), moved 2026-10-07. Its
answered questions are kept below as the design.*

## Current behavior

Built: the ground's baked shadow map (each chunk's MCSH, darkening the
ground to 60%, in the alpha of the tile's blend picture,
`src/038-ground.c`), and soft round "blob" shadows under characters
(the eight nearest each frame, fading with height).

Not built: real-time shadows from the sun, torch and spell light reaching
the world, and their settings. Decided to build in this engine after the
move (wow-chat-2026 173v, answer 11).

## Intended behavior

Shadows as the original has them, then beyond it (the owner, 2026-10-07:
"we should absolutely invent real-time shadows"):
- **Live shadows from the sun:** before each frame the scene is drawn
  from the sun's side keeping only depth (a "depth picture"); each pixel
  of the real frame asks it whether something stood between it and the
  sun. Trees, buildings, creatures and she herself cast shadows that move
  with the time of day. Everything needed is loaded already: every
  shape's points and the sun's direction from the zone's light (306).
  On the graphics card.
- **Torch and spell light:** every model's lights (a torch in a sconce
  or a hand, a spell's glow) light the whole world around them, not
  just their own model. A shadow spell's light is negative: it subtracts
  light within its reach. The shaders add up the nearest few lights per
  pixel.
- **Settings** (accepted 2026-10-07): one Shadow Quality (the depth
  pictures' size, how many cascades), then one choice per source, each
  listing only the forms that exist: the world baked / live; characters
  blob / live; torches and spells off / light only / light and shadows
  (the nearest few). With all off, the client draws the original's look,
  and the pixel-for-pixel comparisons run that way.

## Suggested implementation steps

1. The sun's depth picture and the shadow test in the ground, doodad and
   figure shaders, behind the world setting.
2. Characters in the depth picture (their setting).
3. Model lights read (with 401) and summed per pixel; negative light.
4. Shadows from the nearest torches.
5. The settings in the store; the effects panel.

## Open questions

1. Are a model's own lights, in the original, limited to that model? (As
   far as known, not checked: they don't light the ground and walls.)
   This only matters for comparing; the design above lights the world
   either way.

## Related

- [306](./306-light-and-sky.md), [1001](./1001-the-options-menu.md).
