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

Left: doors and moving objects (game objects come with the server).

## Intended behavior

She collides with the world's models as in the original. The server
doesn't re-check this (it trusts the client's position), so the
client's own collision is what keeps her honest; and when the world's
shape changes while playing, it changes only from the server
(docs/001, "Who holds the truth").

## Suggested implementation steps

1. Game objects (doors, elevators) from the server, moving their
   triangles.

## Related

- [801](./801-movement-by-the-servers-rules.md), [401](./401-m2-reader.md).
