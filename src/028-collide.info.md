# 028-collide

*Written by `scripts/generate-info` from `028-collide.h`; don't edit, re-run it.*

028-collide.h (issue 173q) - the world's solid models, for the mover

For a general audience: every tree, rock, wall, floor and bridge has a
solid shape: a set of triangles (simpler than what is drawn). This keeps
all of them, placed in the world, filed in a grid of 4-yard squares so
a question about one spot only looks at the triangles near it, and
answers the mover's two questions:
  the floor: what is the highest surface under (x, y) at or below a
    height (her feet plus a step), and how does it slope?
  walls: does moving from here to there run into something too steep to
    stand on, at her knees or above? If so, which way does it face, so
    she can slide along it?
The map's own ground is not here: the caller compares the two (043-main.c).

Positions are kept relative to the drawing origin (ox, oy), as the
drawn models are, so single-precision numbers stay exact; questions
are asked in world coordinates and moved here.

## What it offers

- `typedef struct CollideWorld CollideWorld;`
- `CollideWorld *collide_new(double ox, double oy);`
- `void collide_free(CollideWorld *w);`
- `void collide_add(CollideWorld *w, const float *tris, int ntri, const float m[16], int tile);`
  Adds `ntri` triangles (9 floats each: three corners, model space)
  placed by `m` (4 x 4, Raylib's layout as in 020-adt.h, relative to the
  origin), brought by map tile `tile` (tx * 64 + ty; 173s).
- `void collide_drop_tile(CollideWorld *w, int tile);`
  Forgets every triangle tile `tile` brought (the tile is dropped, 173s).
- `float collide_floor(CollideWorld *w, double x, double y, double z_top, float *gx, float *gy);`
  The highest surface under (x, y) at or below `z_top`: its height, and
  its rise per yard along x and y; below -10000 when there is none.
- `float collide_ray(CollideWorld *w, double x0, double y0, double z0, double x1, double y1, double z1);`
  The nearest solid triangle along the straight line from (x0, y0, z0)
  to (x1, y1, z1): the share of the way (0 to 1) where it is struck, or
  above 1 when nothing is in the way. For the camera (173m: it comes in
  to stay on her side of walls, trees and roofs).
- `int collide_count(const CollideWorld *w);`
  How many triangles are kept (for the start-up report and the tests).
