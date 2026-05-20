# 308 — Phase 3 Capstone Demo

## Current behavior

Phase 3 not started. No terrain, no world geometry rendering.

## Intended behavior

The Phase 2 viewer is upgraded into a free-fly world preview that:

- Loads the Azeroth WDT and a 5×5 grid of ADTs around a configured
  spawn point (default: Northshire Abbey).
- Streams ADT textures + alpha maps as the camera approaches each tile.
- Renders terrain with up to 4-layer texture splatting per chunk.
- Renders all WMOs referenced by the loaded ADTs (Northshire chapel,
  abbey, etc.).
- Renders the skybox + sun based on `Light.dbc`.
- HUD adds: tile count loaded, draw call count, frustum-culled chunks,
  triangles submitted.

## Suggested implementation steps

To be expanded into per-issue tickets (301–307) when Phase 3 begins.
Sketch:

- 301 — ADT parser.
- 302 — Terrain mesh builder + splat shader.
- 303 — WDT parser + tile streaming policy.
- 304 — WMO parser (root + groups).
- 305 — WMO renderer with portal-aware culling.
- 306 — Light.dbc + skybox + directional light.
- 307 — Free-fly camera + frustum culling.
- 308 — This capstone demo.

## Acceptance

- `./run-phase-demo.sh` → choose 3 → camera spawns at Northshire,
  user can fly anywhere in the loaded 5×5 grid, terrain + WMOs visible
  and recognizable to anyone familiar with 3.3.5a.

## Related

- Blocked by: Phase 2.
