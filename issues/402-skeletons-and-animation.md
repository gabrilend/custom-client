# 402 — Skeletons and Animation

*Came from: wow-chat-2026 173i (her animation, completed there) and 173j
(figures standing around, completed there), moved 2026-10-07.*

## Current behavior

Built:
- **Reading** (`src/023-anim.c`): a model's bones (parents, pivots,
  flags, camera-facing bones), its animations (ids, variations, lengths,
  speeds, blend times, aliases followed), the tracks of each bone (moves,
  turns, stretches, with global sequences), and the animations kept in
  `.anim` files beside the model; picture-position animations and
  animated colours.
- **Posing:** a bone's pose at any moment, blended between recorded
  moments; parent times local.
- **Bending on the graphics card:** each point's four bones and weights
  go to the card once; each frame only the bone poses go, as a one-row
  float picture; the shader bends (`041-figure.c`, `042-doodads.c`).
  Camera-facing bones turned per copy.
- **Choosing her animation** (`src/030-moves.c`): stand with fidgets,
  run, walk (walk toggled), walk backwards, the turning shuffle, strafing
  with her body turned, jump start, loop and landing, fall, swimming;
  moving animations paced by the speed the keys ask for, so she runs on
  the spot against a wall; changes blended.
- **Figures** (`src/041-figure.c`): any animated model (a character, a
  creature display, an item) posed and bent each frame; bystanders cycle
  their animations.
- Tests: her skeleton reads, she stands on the ground at her height, the
  run loops, the choices pick the right animation at the right pace.

## Intended behavior

Every model moves as the original moves it, and the same layer plays
WC3 models' animations with WC3 timing (walk speed matching, damage
point timing, decay: the WC3 behaviour layer of Phase 4), so one
animator serves both games.

## Suggested implementation steps

1. WC3 animations through the same posing (with 401's MDX reader).
2. The WC3 behaviour layer: walk speed matching, attack damage points,
   death and decay.

## Related

- Posing runs as stations on the ceramic engine: [110e](./110e-per-frame-work-as-stations.md).

- [401](./401-m2-reader.md), [404](./404-characters-and-equipment.md).
