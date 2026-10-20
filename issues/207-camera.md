# 207 — Camera

*Filled in part by the code from wow-chat-2026 (its 173m, 173b4), 2026-10-07.*

## Current behavior

Built (`src/031-camera.c`, window-free, tested): left drag orbits,
right drag steers her; the original's four following styles as a table
of behaviours; Auto Follow Speed, Mouse Look Speed, Invert Mouse, Max
Camera Distance, Follow Terrain, Water Collision, Smart Pivot, Head Bob;
zoom to first person; the camera comes in before walls. Not built: the
saved views and camera key bindings (with key bindings), the WC3
top-down camera (Phase 11).

Measured against the clean-room checklist
([docs/057](../docs/057-what-the-original-program-decides.md), section 3,
filled 2026-10-16), the following camera differs from the original:
- **Zoom:** this client multiplies the distance by 0.85 or 1.18 a wheel
  click and holds a nearest distance of 2 yards before first person.
  The original moves 1 yard a click (0.2 in the barber shop) at a steady
  8.33 yards a second, with no nearest distance above 0; further clicks
  the same way add to the motion under way, and a click the other way
  stops it first (row "one wheel click").
- **Distance:** the original's default is view 2, 5.55 yards back at
  10° down; the farthest is 15 yards × the max-distance factor, never
  above 50 (row "default, closest and farthest", note 3.1).
- **Pitch range:** this client's −0.4 to 1.45 radians (about −23° to
  83°); the original's is ±89°.
- **Mouse look:** this client turns 0.006 radians (0.34°) a pixel at the
  default; the original 0.225° a unit sideways and 0.15° up and down
  (row "mouse-look").
- **Walls:** the original pulls in to 1/9 yard short of the hit, after
  first finding the eye height with a check upward from 0.833 yard, and
  keeps off water surfaces (note 3.2).
- **Smoothing:** this client's table holds the four styles the menu
  lists (1, 4, 2, 0) as which way the camera drifts, and calls style 1
  the default. The original has five (3, "Spline", isn't in the menu),
  its default is **4** ("Smarter"), and each style is a delay and a
  strength for each of seven states (Idle, Stop, Track, Move, Strafe,
  Turn, Fear), with turning at 180° a second and pitching at 45° a
  second (note 3.3). The style setting accepts 5, which means nothing
  (a **quirk**: kept, with a correction switch that refuses it).

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

### The original's camera (from docs/057 section 3)

Each step links back to its row or note in docs/057 when done.

8. Zoom as a steady motion at `cameraDistanceMoveSpeed`, 1 yard a click,
   down to 0 (first person) with no nearest step; distances from the
   eight view presets (note 3.1) and the farthest from
   `cameraDistanceMax` × `cameraDistanceMaxFactor`, at most 50.
9. Pitch held to ±89°, settable by `pitchLimit`.
10. Mouse look by the original's degrees per unit (row "mouse-look").
11. Walls and water by note 3.2: the eye height check, the pull-in to
    1/9 yard short, water kept at 2/9 yard above or 0.833 below, the
    diving pitch. The lowest eye height reads the body's height from
    [802](./802-collision.md).
12. Smoothing as five styles of seven per-state delays and strengths
    (note 3.3), default 4; the existing drift table becomes the reading
    of those numbers rather than its own guess.
13. The settings in note 3.4 with their defaults and allowed ranges.
14. Tests: one click moves 1 yard in 120 ms; two quick clicks add up;
    the pitch stops at 89°; a wall 3 yards behind her puts the camera at
    2.889 yards.

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
- **Blocks:** [1003](./1003-sounds-in-the-world.md) (the listener).
