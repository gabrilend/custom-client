# 308 — Phase 3 Capstone Demo

## Current behavior

Phase 3 not started. No terrain, no buildings, no sky — just the
flat-sprite demo from Phase 2.

## Intended behavior

A free-fly world preview rooted at Northshire Abbey, exercising every
Phase 3 subsystem end to end:

- Loads `Azeroth.wdt` via [303](./303-wdt-and-streaming.md).
- Streams a 5×5 grid of ADTs around the camera's current tile via the
  streamer thread; ADTs slide in and out as the camera moves.
- Renders terrain with the 4-layer splat shader from
  [302](./302-terrain-renderer.md). Recognizable Elwynn ground
  textures: grass, dirt patches, the road, riverbed gravel.
- Renders every WMO referenced by the loaded ADTs through
  [305](./305-wmo-renderer.md). Northshire chapel and abbey are visible
  and recognizable; flying inside, the interior groups light up via
  vertex colors while exterior groups disappear from the render set
  via portal-walk culling.
- Lit by `Light.dbc` ([306](./306-light-and-sky.md)) sampled at noon —
  Elwynn's characteristic warm green tint.
- Skybox blends from horizon to zenith colors with a billboarded sun
  disk in the direction of `-sun_direction`.
- [Free-fly camera](./307-freefly-and-culling.md) for navigation; ESC
  unlocks the cursor for HUD interaction; F3 draws the frustum
  wireframe for debugging.
- No characters yet — Phase 4 fills in M2 doodads and player models.
  The HUD says so explicitly.

### HUD stats — per CLAUDE.md, this is what the demo is for

- Frame time + FPS + 1%-low FPS over the last 5 seconds.
- Camera world position (xyz), tile (tx, ty), yaw, pitch.
- Tiles resident / in flight / evicted-this-second.
- Terrain chunks drawn / chunks frustum-culled / triangles submitted.
- WMOs resident / WMO groups drawn / WMO groups portal-culled.
- Doodad refs *would-have-drawn* (counts the M2 placements Phase 4
  will fill in).
- Bytes streamed this second.
- GPU memory used (terrain VBOs + WMO VBOs + textures).
- Active `Light.dbc` zones at the camera position with per-zone
  weight.
- Driver: vendor, renderer, GL version, S3TC, max texture units.

## Suggested implementation steps

1. Add `src/demos/phase-3/`. Build into `phase-3-demo`.
2. Write `issues/completed/demos/phase-3.sh`.
3. Extend `run-phase-demo.sh` to dispatch to phase 3.
4. The demo's `main`:
   - Load config, log, window, MPQ chain.
   - Init: shaders, BLP cache, terrain renderer, WMO renderer,
     skybox, lighting db, world stream against Azeroth.wdt.
   - Position the free-fly camera at the Northshire human spawn coords
     (well-known: roughly `(-8924, -132, 81)` in 3.3.5a world coords —
     verify against an in-server `.gps` printout).
   - Main loop: input → freefly update → camera vp → world_stream tick
     → frustum_from_view_proj → skybox draw → terrain draw → wmo draw
     → HUD.
5. On quit, write `output/goodbye.txt` summarizing the session
   (uptime, frames rendered, tiles loaded, bytes streamed).

## Acceptance

- `./run-phase-demo.sh` → choose 3 → window opens at Northshire,
  recognizable to anyone who has played the original at the same spot.
- Flying around the 5×5 streamed area maintains > 60 FPS on a modest
  GPU; tile streaming is invisible (no frame-time spikes on tile
  boundaries).
- ASan-clean across a 5-minute fly-around session.
- Per CLAUDE.md ("phase demos should focus on demonstrating relevant
  statistics or datapoints"): the HUD is the deliverable as much as
  the visuals are.

## Related

- All of 301–307 are blockers.
- Becomes a permanent deliverable.
