# 307 — Free-Fly Camera and Frustum Culling

## Current behavior

The Phase 2 [3D camera stub](./207-camera.md) builds a view+projection
matrix but is not user-driven. There is no frustum-plane extraction, so
the terrain renderer and WMO renderer are still drawing every chunk
and every group regardless of where the camera looks.

## Intended behavior

Two things.

### Free-fly controls

A small input-driven camera controller that:

- **WASD** translates along the camera's right/forward axes,
- **Space / Ctrl** lift / drop along world +Z,
- **Mouse look** rotates (yaw + pitch, clamped to ±89°),
- **Shift** multiplies speed ×4 (a "fast travel" shortcut for the demo),
- **Mouse wheel** adjusts the base speed,
- **F1** toggles the free-fly controller on/off (off → cursor unlocked
  for HUD interaction; on → cursor captured).

```c
typedef struct freefly freefly_t;

err_t freefly_init(freefly_t **out);
void  freefly_close(freefly_t *);

void  freefly_update(freefly_t *, camera3d_t *, float dt);
void  freefly_set_position(freefly_t *, vec3_t pos);
```

Implementation leans on raylib's input API directly (`IsKeyDown`,
`GetMouseDelta`, `DisableCursor` / `EnableCursor`). No need for an
input-abstraction layer in Phase 3 — when we get to FrameXML the input
routing changes shape anyway, and we'll design that layer then.

### Frustum extraction + tests

```c
typedef struct {
    vec4_t planes[6];          /* left, right, bottom, top, near, far */
} frustum_t;

frustum_t frustum_from_view_proj(mat4_t vp);

bool      frustum_test_aabb (const frustum_t *, aabb_t);
bool      frustum_test_sphere(const frustum_t *, vec3_t center, float r);
```

Plane extraction is the standard "rows of the view-projection matrix
combined into plane equations" trick (Gribb & Hartmann, 2001). The
output planes are normalized (so `dot(plane.xyz, p) + plane.w` is
signed distance in world units).

The AABB test uses the "positive vertex" optimization — pick the corner
of the AABB most in the direction of the plane normal and test it; if
even that corner is behind the plane, the whole box is outside.

[302](./302-terrain-renderer.md) and [305](./305-wmo-renderer.md) call
`frustum_test_aabb` per chunk / per group as their first cheap reject.

## Suggested implementation steps

1. Create `src/freefly/` subsystem with the controller.
2. Add `frustum_t` + `frustum_from_view_proj` to the `src/math/`
   subsystem (it's geometry on `mat4_t`, belongs there).
3. Implement the AABB + sphere tests with the positive-vertex
   shortcut.
4. Wire `freefly_update` into the demo loop: call it before
   `camera3d_view_proj` each frame.
5. Add HUD stats: camera position (xyz), camera tile (tx, ty), yaw,
   pitch, speed, fov, frustum-extracted plane normals for debugging.
6. Tests:
   - Plane extraction: build a known view+proj, extract planes, assert
     a known point inside and outside.
   - AABB test: an obviously-outside box returns false; an obviously-
     inside box returns true; a box crossing one plane returns true.
   - Free-fly: simulate 60 frames of WASD input and assert the camera
     position moved in the expected direction.

## Acceptance

- The Phase 3 demo lets the user fly anywhere they like, including
  straight up high enough to see the streamed tile window from above.
- Per-frame stats show "chunks culled" climbing as the camera turns
  away from terrain.
- Looking straight down with a tight FOV culls > 95% of WMO groups
  outside the cone.

## Stress markers

Mouse-look feels wrong if `pitch` is computed in the wrong order
relative to `yaw`. Quaternion-composed rotation in the order
`yaw_then_pitch` (yaw around world +Z, then pitch around the
camera's local right axis) gives the FPS-style feel; do *not* try to
roll by accumulating a single quaternion, because float drift will
introduce roll over time. Build the basis from yaw + pitch fresh each
frame.

Frustum culling can silently *under*-cull (drawing things we could
skip — wastes time, not visible) or *over*-cull (skipping things we
shouldn't — visible pop). Over-culling is the disastrous one; if the
positive-vertex test ever returns false for an AABB that the renderer
would otherwise draw correctly, holes appear at the edge of the
screen. Have a debug toggle (`F3`) that draws the frustum planes as
wireframe so a misculled chunk is obvious.

## Related

- [202](./202-math-primitives.md), [207](./207-camera.md)
- Blocked by: 207.
- Blocks: [308](./308-phase-demo.md). Speeds up 302 and 305 enough that
  the demo holds frame rate.
