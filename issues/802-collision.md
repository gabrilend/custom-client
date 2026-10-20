# 802 — Collision with the World's Models

*Came from: wow-chat-2026 173q (model collision), moved 2026-10-07.*

## Current behavior

Built (`src/028-collide.c`, window-free):
- Each small model's collision triangles and each building's triangles
  its MOPY flags mark solid, placed with the same matrices as drawn
  (checked against the server's own placements: the placement test in
  401), filed into a grid of 4-yard squares.
- **Floors:** the highest surface at or below her feet plus a step (the
  map's ground, a bridge, a floor, a stair).
- **Walls:** moving into a surface too steep to stand on stops her short
  of it and slides her along it.
- **Lines of sight:** the camera comes in to just before anything solid
  between her and it.
- Tiles' triangles dropped with their tile.
- Tests: she walks onto a raised floor, stops at a wall, slides along
  it, and a dropped tile forgets only its own triangles.

Her body is a round post 0.4 yards across the middle (a guess). The
clean-room checklist
([docs/057](../docs/057-what-the-original-program-decides.md), filled
2026-10-16) says the original's is different (row "each race's
collision body", note 2.2):
- **Size** from the creature-model-data table's collision width and
  height for her own display model, times the model's scale (0.667 by
  2.028 yards when there is no data).
- **Shape:** square from above, not round, half the width out along
  each axis. From the side, a box on top of an upside-down square
  pyramid whose tip is at her feet; the box's bottom is 1.849 × the half
  width above the feet. The point is what lets her settle onto slopes
  and step edges instead of hanging on a corner.
- **Mounted:** the mount's height × its scale + half her own height,
  never less than her own; the width stays hers. A body that would not
  fit where she stands is refused (no mounting under a low roof).
- **Walkable** ground is at most 50° from level for her, but **80°** for
  units this client doesn't control (other players and creatures as
  drawn here), so they never visibly slide where she would.
- **Stepping up** looks ahead by the larger of her half-width plus 1/720
  yard, or 1.19 × the step height.

Left: that body; doors and moving objects (game objects come with the
server).

## Intended behavior

She collides with the world's models as in the original. The server
doesn't re-check this (it trusts the client's position), so the
client's own collision is what keeps her honest; and when the world's
shape changes while playing, it changes only from the server
(docs/001, "Who holds the truth").

## Suggested implementation steps

1. The original's body (docs/057 note 2.2): its size read from the
   tables per display model, the box-on-a-pyramid shape in the wall and
   floor tests, the mounted height, refusing a body that doesn't fit.
   A model with collision width under 0.4167 yard is a data warning; a
   zero width or height is an error and the body is left as it was.
   801's swimming depth and the camera's lowest eye height (207) both
   read this body's height. Other units stand on slopes up to 80°; the
   step-up look-ahead as above.
2. Tests: she stands still on a step edge her middle overhangs (the
   pyramid's tip settles); a mounted body taller than a doorway is
   refused under it; a square body touches a wall at its corner on a
   diagonal approach where a round one would not.
3. Game objects (doors, elevators) from the server, moving their
   triangles.

## Related

- [801](./801-movement-by-the-servers-rules.md), [401](./401-m2-reader.md).
