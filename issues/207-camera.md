# 207 — Camera

## Current behavior

raylib ships `Camera2D` and `Camera3D` types with `BeginMode2D` /
`BeginMode3D` helpers that drive its default shaders. They're handy for
prototyping but don't expose the view/projection matrices we need to
hand to our own shaders, and `Camera3D` assumes +Y up (we use +Z up).
There is no project camera concept yet.

## Intended behavior

A camera subsystem that:

- Owns our own `view_t` and `projection_t` matrices (always available
  for any shader, our own or raylib's).
- Provides two flavors: a 2D ortho camera for the UI pass and a 3D
  perspective camera for the world pass.
- Builds the raylib `Camera2D` / `Camera3D` struct on demand for code
  that needs them (`BeginMode3D`-using paths). Bridges are explicit,
  same pattern as the math layer's raylib helpers.
- For Phase 2, only the 2D camera is exercised end-to-end. The 3D
  camera lands as a stub: types defined, `mat4_perspective` +
  `mat4_look_at` plumbed in, but no scene yet to point it at. Phase 3
  fills it in.

```c
typedef struct camera2d camera2d_t;
typedef struct camera3d camera3d_t;

void   camera2d_set_target(camera2d_t *, vec2_t target);
void   camera2d_set_zoom  (camera2d_t *, float zoom);
mat4_t camera2d_view_proj (const camera2d_t *, int viewport_w, int viewport_h);

void   camera3d_set_eye   (camera3d_t *, vec3_t eye);
void   camera3d_set_target(camera3d_t *, vec3_t target);
void   camera3d_set_fov   (camera3d_t *, float radians);
mat4_t camera3d_view_proj (const camera3d_t *, int viewport_w, int viewport_h);

Camera2D camera2d_to_raylib(const camera2d_t *);
Camera3D camera3d_to_raylib(const camera3d_t *);   /* with axis-swap to +Y */
```

**Two 3D controllers will drive `camera3d_t`** (W client merge,
2026-09-23): the free-fly/third-person camera of WoW world mode
([307](./307-freefly-and-culling.md)), and the **top-down RTS camera** of WC3
map mode (Phase 11): a fixed steep pitch (WC3's default is roughly 56° from
horizontal, looking down), zoom by changing distance, and pan by screen edge
or arrow keys. Nothing in `camera3d_t` assumes which one is in use; the
math in this issue covers both. The owner's reason for this: the stock
client can't look straight down, and "the purpose of creating a custom
client is so that we can heavily modify it."

The `camera3d_to_raylib` bridge does the +Z-up → +Y-up axis swap
explicitly. There is exactly one place where that swap happens, named
in code, with a comment explaining why.

## Suggested implementation steps

1. Create `src/camera/` subsystem.
2. Implement `camera2d_t` (target + zoom + rotation, ortho projection).
3. Implement `camera3d_t` (eye + target + up + fov + near/far,
   perspective projection).
4. Build the view-projection matrices using the math primitives from
   [202](./202-math-primitives.md).
5. Implement the two raylib bridge functions, with the axis-swap in
   `camera3d_to_raylib` accompanied by the comment:

   ```
   /* Our world is +Z up. raylib's default camera is +Y up. We swap
      here at the boundary so the rest of the world stays consistent
      with the asset coordinate system. */
   ```
6. Wire the Phase 2 demo to use `camera2d_view_proj` when calling into
   the sprite renderer.
7. Tests: project a known world point through `camera3d_view_proj` and
   confirm the resulting clip-space coordinate. Round-trip a 2D point
   through `camera2d_to_raylib` → `GetWorldToScreen2D` → our own
   inverse and confirm the same answer.

## Acceptance

- The Phase 2 demo pans + zooms a sprite with mouse drag + wheel,
  driven by `camera2d_t`.
- A unit test confirms a known eye/target/up combination produces a
  view matrix equal (to float epsilon) to a hand-computed reference.

## Stress markers

Coordinate-system bugs are silent until they aren't. The single explicit
axis-swap site is the only insurance against that — the temptation to
"just use raylib's Camera3D directly here, this one time" will be
strong; resist it.

## Related

- [202](./202-math-primitives.md)
- [docs/005-rendering-pipeline.md](../docs/005-rendering-pipeline.md)
- Blocked by: [202](./202-math-primitives.md), [201](./201-window-and-context.md).
- Blocks: [208](./208-phase-demo.md), all of Phase 3.
