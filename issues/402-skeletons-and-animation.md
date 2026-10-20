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

Measured against the clean-room checklist
([docs/057](../docs/057-what-the-original-program-decides.md), section 5,
filled 2026-10-16), where this differs from the original:
- **Which moving animation:** chosen here from the keys (back, walk
  toggled, else run). The original chooses by a fixed order of eight
  rules (note 5.3): swimming strafes, swimming, flying paths, backward,
  stealthed (stealth walk), **11 yards a second or more (sprint)**,
  above twice the walk speed (run), else walk. On the ground, strafing
  has no animation of its own there.
- **Blending:** here a straight-line mix over the new animation's blend
  time, or 150 ms when it has none. The original mixes along an S-curve
  (3t² − 2t³), and keeps a blend under way when the outgoing animation
  still carries more than half the weight, so a quick change doesn't
  snap (note 5.4). What it does for a blend time of 0 isn't said.
- **A missing animation:** here only the model's own aliases are
  followed (8 hops). The original follows the animation-data table's
  fallback link by link, stopping on a loop, a self-link or a missing
  record, then uses stand, or "closed", or the first animation; each
  link can mark it reversed (flag 16) or frozen (flag 32) (note 5.4).
- **Repeats** a random whole number of times between the model's least
  and most for that animation; **variations** picked by frequency out of
  32768 (here only for the stand's fidgets); **secondary animations**
  (the upper body) at most 75% of the weight.
- **Combat stance** (note 5.8): not built here. Chosen by the main-hand
  weapon, else the off-hand weapon: one-handed weapons, daggers, fist
  weapons and fishing poles "ready, one-handed"; two-handers "ready,
  two-handed" (one-handed if anything is in the off hand); polearms,
  staves and spears "two-handed long"; everything else, and no weapon,
  "ready, unarmed".
- **Jumping and landing** (note 5.8): this client plays jump start, loop
  and land. The original lands with jump-end only when standing still;
  moving forward or sideways faster than twice walking speed, it plays
  "land while running"; otherwise no landing animation at all. A
  knockback plays the fall animation; taking off into flight plays jump
  start, and stopping flight plays "land while running". The jump and
  land sounds play with them ([1003](./1003-sounds-in-the-world.md)).

## Intended behavior

Every model moves as the original moves it, and the same layer plays
WC3 models' animations with WC3 timing (walk speed matching, damage
point timing, decay: the WC3 behaviour layer of Phase 4), so one
animator serves both games.

## Suggested implementation steps

1. The original's choices (docs/057 notes 5.3 and 5.4; link back to them
   when done): the eight-rule choice of moving animation by speed and
   state; the fallback chain through the animation-data table with its
   reverse and freeze marks; the S-curve blend that keeps a blend under
   way; repeats and weighted variations for every animation; secondary
   animations capped at 75%.
2. Tests: a speed of 11 yards a second picks sprint, 10.9 run, twice
   walk speed exactly picks walk; a model lacking an animation lands on
   its table fallback; a fallback chain that loops ends on stand; two
   changes 50 ms apart don't snap.
3. Combat stance and the landing choice (note 5.8). Tests: landing
   while running at run speed plays "land while running"; landing while
   walking plays none; landing standing plays jump end; a two-handed
   sword with a shield in the off hand gives "ready, one-handed".
   Emote animations still wait on docs/057. Weapons, sheathing and
   riding are in [404](./404-characters-and-equipment.md).
4. WC3 animations through the same posing (with 401's MDX reader).
5. The WC3 behaviour layer: walk speed matching, attack damage points,
   death and decay.

## Related

- Posing runs as stations on the ceramic engine: [110e](./110e-per-frame-work-as-stations.md).
- **Blocks:** [1003](./1003-sounds-in-the-world.md) (the model events that fire sounds), [811](./811-spell-visuals.md) (casting, wound and death animations).

- [401](./401-m2-reader.md), [404](./404-characters-and-equipment.md).
