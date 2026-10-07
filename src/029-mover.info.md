# 029-mover

*Written by `scripts/generate-info` from `029-mover.h`; don't edit, re-run it.*

029-mover.h (issue 173k; the C twin of lib/mover.lua) - how a character moves

For a general audience: one step of a character's movement, given what
the player is pressing and how much time passed: turning, walking along
the ground, falling, jumping, and sliding down ground too steep to
stand on. The numbers are the game's own base speeds, times the speed
bonus (the owner asked for 120%). No drawing here, so it is tested alone.

Facing is the game's: radians, 0 = north (+x), rising toward west (+y).

## What it offers

- `type Mover`
  how deep her feet are below the surface when she swims at it, and so
  how deep water must be over her feet before she swims rather than
  wades (173r; the original's own number is per model and not yet read:
  this is about her shoulders' height below her head)
- `type MoverInput`
- `typedef float (*MoverGround)(void *ctx, float x, float y, float z_top, float *gx, float *gy);`
  The ground at (x, y): the highest surface at or below `z_top` (the
  map's ground, or a model's floor: a bridge, a stair, 173q), its height
  (below -10000 for none: a hole, or the edge of the loaded world), and
  its rise per yard along x and along y.
- `typedef int (*MoverWall)(void *ctx, double x0, double y0, double x1, double y1, double z, float *nx, float *ny);`
  Walls (173q): whether a step from (x0, y0) to (x1, y1), feet at z,
  runs into something too steep to stand on; if so, its facing along the
  ground in (nx, ny). NULL: no walls (the map's ground alone).
- `typedef float (*MoverWater)(void *ctx, double x, double y);`
  Water (173r): the water's surface at (x, y), below -10000 for none.
- `type MoverWorld`
  What the mover may ask of the world; any question may be NULL (no
  walls, no water), except the ground.
- `void mover_init(Mover *m, double x, double y, double z, double o, double speed, double max_slope);`
  max_slope: degrees, the steepest walkable ground (navmesh_slope_read).
- `void mover_step(Mover *m, const MoverInput *in, double dt, const MoverWorld *world);`
- `double mover_intended_speed(const Mover *m, const MoverInput *in);`
  The speed (yards a second) the keys held ask for, whatever stops her:
  walking, running, backwards or swimming, times her speed bonus; 0 with
  no movement key. Her animation is paced by this, not by how far she
  really went, so she runs on the spot against a wall, as the original
  does (owner, 2026-10-07).
