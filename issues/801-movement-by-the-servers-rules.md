# 801 — Movement by the Server's Rules

*Came from: wow-chat-2026 173a (first light), 173i (walking), 173r
(swimming) and 173e (what the server checks), moved 2026-10-07.*

## Current behavior

Built (window-free, tested alone):
- **The ground the server uses** (`src/018-maps.c`): the server's own map
  files, with the server's own arithmetic for height and steepness, so
  she stands where the server thinks she stands. Shared by two threads
  under a lock in the world it belongs to.
- **The slope rule** (`src/019-navmesh-slope.c`): the steepest walkable
  slope, from the server's navigation tiles.
- **One step of movement** (`src/029-mover.c`): the game's own speeds
  times her bonus (run 7, back 4.5, walk 2.5, swim 4.72 yards a second),
  turning, strafing, jumping, falling, sliding down ground too steep;
  swimming in deep water (up with the jump key, down with X, forward
  following the camera's pitch), walking out onto a shore. Floors and
  walls from the models' shapes (802). The speed the keys ask for, for
  the animation's pace.
- **Controls:** W/S, A/D (strafing with the right button), Q/E, Space,
  the keypad's / to walk; both buttons run forward.
- Tests: a second forward is 8.4 yards; back 5.4; walking 3 every way;
  jumps rise and land; slopes and cliffs; swimming and floating.

Left: the original's own numbers for her step height, body radius and
swim depth (open questions); telling the server what she does (Phase 5):
movement packets and acknowledgements exactly as the original sends them
(wow-chat-2026 173e and 173l).

## Intended behavior

She moves exactly as the server expects (wow-chat-2026 173e): the same
heights, speeds, slope limit and falls, so a server with its checks on
never corrects her.

## Suggested implementation steps

1. Measure the original's step height, body radius and swim depth;
   replace this client's guesses.
2. The movement packets (Phase 5), checked against the original's bytes
   (wow-chat-2026 173l's tap).

## Open questions

1. Her step height and body radius: the original's numbers, unmeasured;
   this client uses 1 yard and 0.4 yards.
2. How deep the water must be over her feet before she swims, and how
   deep she floats: unmeasured; this client uses 1.6 yards (docs/046).

## Related

- [802](./802-collision.md), [309](./309-water.md), [402](./402-skeletons-and-animation.md).
