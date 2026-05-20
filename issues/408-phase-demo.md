# 408 — Phase 4 Capstone Demo

## Current behavior

Phase 4 not started. No animated characters in the world.

## Intended behavior

The Phase 3 viewer gains animated M2 models:

- Loads the human male M2 + its `.skin` + default animations.
- Renders it at the spawn point, playing the idle animation in a loop.
- Cycles through walk/run/jump/death animations on number-key press.
- A few NPC M2s scattered around the loaded ADTs for variety.
- HUD adds: bone matrix updates per frame, particles alive, ribbons
  alive, M2 visible / culled counts.

## Suggested implementation steps

To be expanded into per-issue tickets (401–407) when Phase 4 begins.
Sketch:

- 401 — M2 parser (verts, bones, sequences, textures, render flags).
- 402 — `.skin` parser (submesh table, LOD index buffers).
- 403 — Skeleton evaluation + bone matrix upload.
- 404 — M2 renderer (opaque + transparent passes).
- 405 — Particle emitters.
- 406 — Ribbon emitters.
- 407 — Texture variant + item display info hookups (CreatureDisplayInfo).
- 408 — This capstone demo.

## Acceptance

- `./run-phase-demo.sh` → choose 4 → recognizable human male standing
  in Northshire, looping the canonical idle, switchable to other anims.

## Related

- Blocked by: Phase 3.
