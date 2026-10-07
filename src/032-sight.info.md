# 032-sight

*Written by `scripts/generate-info` from `032-sight.h`; don't edit, re-run it.*

032-sight.h (issue 173u) - what the camera can see

For a general audience: the camera sees a wedge of the world (wider
farther off, cut off near and far). Drawing anything outside it is
wasted work the graphics card throws away, and every draw order costs
the processor. This keeps the wedge's six walls for the current frame
and answers "could a ball this big, here, be seen?", so tiles, doodads
and creatures outside the view aren't sent at all.

Each wall is a plane: a, b, c, d with a*x + b*y + c*z + d >= 0 inside
(unit length a, b, c), in drawing coordinates (the world less the
drawing origin, as everything drawn is).

## What it offers

- `type Sight`
- `void sight_from(Sight *s, Matrix view_projection);`
  The wedge seen through `view_projection` (the camera's view matrix
  times its projection, Raylib's layout).
- `int sight_ball(const Sight *s, Vector3 centre, float radius);`
  1 when a ball at `centre` with `radius` reaches into the wedge.
