# 408 — Phase 4 Capstone Demo

## Current behavior

Phase 4 not started. No animated characters in the world.

## Intended behavior

The Phase 3 viewer gains animated M2 models, in both modes:

- **WoW world:** the human male M2 + `.skin` + default animations at the
  Northshire spawn point, idle looping, number keys cycle walk/run/jump/death,
  a few NPC M2s scattered around.
- **WC3 map mode preview:** a converted WC3 map (overlay folder) with a
  footman-class unit that stands, walks with feet matched to its WC3 speed,
  swings so the hit lands at WC3's damage point, dies, lies as a corpse and
  sinks away on WC3's decay timer. Team colour shown.
- Models are fetched through the asset resolver. At least one replacement
  model (from world-edit-to-execute's forge, bound to a borrowed Blizzard
  skeleton, W05b) stands beside its original, labelled with its similarity
  score.
- HUD: bone matrix updates per frame, particles alive, ribbons alive, M2
  visible / culled, model sources (pack / blizzard / placeholder), replaced
  models past the distinctness threshold.

## Suggested implementation steps

To be expanded into per-issue tickets (401–407) when Phase 4 begins.
Sketch:

- 401 — M2 parser (verts, bones, sequences, textures, render flags, attachments, cameras); joins `libwreaders.so`.
- 402 — `.skin` parser (submesh table, LOD index buffers) and external `.anim` files.
- 403 — Skeleton evaluation + bone matrix upload.
- 404 — M2 renderer (opaque + transparent passes, team-colour tint).
- 405 — Particle emitters.
- 406 — Ribbon emitters.
- 407 — Display info hookups (CreatureDisplayInfo) and the **WC3 animation
  behaviour layer**: WC3 animation names mapped to WoW sequences through
  `AnimationData.dbc`, WC3 timing rules. Design: world-edit-to-execute
  `docs/datapath-wow-models-in-engine.md`.
- 408 — This capstone demo.

## Acceptance

- `./run-phase-demo.sh` → choose 4 → human male idle in Northshire,
  switchable anims; a WC3 unit walking and dying on WC3 timing; one
  replacement model with its score on screen.

## Related

- Blocked by: Phase 3.
- world-edit-to-execute W03 (the behaviour rules), W05 / W05a / W05b (replacements, scores, skeletons).
