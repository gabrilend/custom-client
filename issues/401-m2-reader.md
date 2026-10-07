# 401 — The M2 Reader

*Came from: wow-chat-2026 173d (models and world objects) and 173g
(doodads), moved 2026-10-07.*

## Current behavior

Built (`src/022-m2.c`, window-free, in the reader library):
- An M2 model and its first view (`<name>00.skin`) read into points
  (position, normal, two sets of picture positions, four bones and
  weights each), pieces (the skin's submeshes), units (each piece's
  pictures, up to two layers, blend mode and flags, which set of picture
  positions each layer uses, picture-position animation, animated
  colour and transparency, the combine code), and the picture list (each
  a named file or a kind the game chooses at run time: skin, hair, cape,
  fur, creature skins).
- The model's collision triangles (header 216 and 224), simpler than
  what is drawn (`m2_collision`).
- Used by doodads and buildings' furnishings (`042-doodads.c`), figures
  (`041-figure.c`), and the reading-ahead thread.
- Tests: her model reads with her known height and triangle count; the
  doodads at Astranaar are placed exactly where the server's own
  collision tiles place them (710 of 710, within 0.0007 yards).

Not yet: lower-detail views (`01.skin` onward), ribbons (403), the
model's own lights (310), cameras inside models.

## Intended behavior

Every M2 the game uses is read as the original reads it, version 264
(3.3.5a), into what the renderer, the animator and the collision need,
with nothing drawn by the reader itself. WC3 models join the same
renderer (docs/001, "One engine, two games"): a second reader beside this
one fills the same structures.

## Suggested implementation steps

1. Lower-detail views, chosen by distance.
2. The WC3 model reader (MDX), filling the same model structure.
3. Ribbon emitters (with 403) and model lights (with 310).

## Related

- [402](./402-skeletons-and-animation.md), [403](./403-particles.md),
  [404](./404-characters-and-equipment.md), [305](./305-wmo-renderer.md).
