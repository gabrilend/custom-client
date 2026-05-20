# 305 — WMO Renderer with Portal-Aware Culling

## Current behavior

[304](./304-wmo-parser.md) gets WMO geometry into CPU memory. Nothing
renders it. The buildings and interiors that should be visible from the
[Phase 3 demo's](./308-phase-demo.md) Northshire spawn point — chapel,
abbey, bridge — are absent.

## Intended behavior

A renderer that draws each WMO as a collection of grouped meshes, with
portal-aware visibility so that interiors don't pay for geometry on the
opposite side of opaque walls.

```c
err_t wmo_renderer_init  (wmo_renderer_t **out);
void  wmo_renderer_close (wmo_renderer_t *);

err_t wmo_renderer_upload(wmo_renderer_t *, const wmo_t *,
                          wmo_handle_t *out);
void  wmo_renderer_unload(wmo_renderer_t *, wmo_handle_t);

void  wmo_renderer_draw  (wmo_renderer_t *,
                          const camera3d_t *,
                          const wmo_placement_t *placements, int n);
```

### Per-group draw

For each group:

- One VBO/IBO range per WMO (uploaded once at `wmo_renderer_upload`).
- For each `wmo_batch_t`, bind the material's texture(s) and shader,
  draw the index range.
- Two shader variants:
  - `wmo_outdoor.{vert,frag}` — directional light + ambient (from
    [306](./306-light-and-sky.md)).
  - `wmo_indoor.{vert,frag}` — MOCV vertex colors as the lighting term,
    plus per-WMO ambient.

### Portal walk

The portal-aware culling step:

1. Determine which group the camera is **inside** (`aabb_contains` for
   each group, narrowing by indoor flag).
2. Start the visible-group set with that group (or all outdoor groups
   if the camera is outside any).
3. For each group in the set, walk its portals (via MOPR). For each
   portal that:
   - Has a normal facing the camera (back-face cull the portal itself), and
   - Has its polygon overlapping the current view frustum,

   add the *other* group of that portal to the set, with the frustum
   tightened to the portal's screen-space bounds.
4. Repeat until no new groups are added.

This is the same algorithm Blizzard's client uses (with minor
variations). It is the reason you can stand inside Stormwind Cathedral
and not pay for rendering the entire Old Town skyline through the
walls.

For v1 the frustum tightening is a simple sutherland–hodgman clip of
the portal polygon against the current frustum; if the clipped
polygon is empty, skip. Don't over-engineer the bounds — a tight 2D
screen-space AABB derived from the clipped polygon is fine.

### Placement transform

A WMO is placed in the world by an `MODF` entry from an ADT
([301](./301-adt-parser.md)) — `(position, yaw/pitch/roll, bbox)`. The
renderer builds the model matrix per placement on the main thread each
frame and supplies it as a uniform.

### Doodad refs stubbed

The MODD doodad placements reference M2 models (Phase 4). For Phase 3
the renderer just *counts* them and reports the count in the demo HUD;
it does not draw them. [305a](./305a-wmo-doodads.md) is the future
sub-issue that fills in M2 doodad drawing once Phase 4 lands.

## Suggested implementation steps

1. Create `src/wmo_renderer/` subsystem.
2. Implement upload: one combined VBO + IBO across all groups of the
   WMO, with a per-group offset table.
3. Write the indoor + outdoor shader pair.
4. Implement the portal walk:
   - Camera-containment test against group AABBs.
   - Per-portal back-face + frustum-overlap test.
   - Iterative widening of the visible-group set until stable.
5. Implement per-group draw: bind material, set uniforms (model-view-proj,
   light direction, ambient color), draw each batch.
6. Stats: WMOs resident, groups drawn, groups portal-culled, batches
   drawn, doodad refs *would-have-drawn*.
7. Tests: portal-walk unit tests on a synthetic 3-room layout (room A
   with a door to B, B with a door to C, walls otherwise) — assert
   that from inside A facing the AB door, B is in the visible set but
   C is not; turn 180°, neither is visible.

## Acceptance

- Northshire chapel renders correctly with both interior (when the
  camera is inside) and exterior shading.
- Standing inside the abbey, the chapel's interior is *not* rendered
  unless visible through a portal.
- The HUD readout confirms portal culling is removing groups (not just
  a fancy outdoor frustum cull).

## Stress markers

The "which group is the camera in" test is more subtle than it looks —
AABBs of adjacent groups can overlap at doorways. Tie-break by which
group's *flags* claim "indoor" and by distance to centroid. A camera
that flips between groups every other frame as it stands in a doorway
will reveal this bug fast.

Portal walks can in principle loop forever if a portal's "other side"
group itself contains a portal back. Use a visited-set to guard the
iteration.

## Related

- [304](./304-wmo-parser.md)
- [docs/005-rendering-pipeline.md](../docs/005-rendering-pipeline.md)
- [docs/003-asset-formats.md](../docs/003-asset-formats.md#wmo--world-map-objects-buildings-dungeons)
- Blocked by: 304, 203, 204, 207.
- Blocks: [308](./308-phase-demo.md).
- Follow-up: `305a-wmo-doodads` (Phase 4).
