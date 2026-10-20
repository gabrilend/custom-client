# 811 — Spell Visuals: Stages and Missiles

*Written 2026-10-16 from the clean-room checklist's notes 6.0, 6.1 and
5.5 ([docs/057](../docs/057-what-the-original-program-decides.md)). The
spell tables name what to show (kits, a missile model, attachment
numbers); the original program decides when each piece starts and
stops, and how a missile flies.*

## Blocked by

- [402](./402-skeletons-and-animation.md) (casting, wound and death
  animations), [403](./403-particles.md) (the kits' particles),
  [404](./404-characters-and-equipment.md) (weapons drawn and held),
  Phase 5 (the cast messages from the server).

## Blocks

- [808](./808-phase-demo.md).

## Current behavior

Nothing exists. Models, animations and particles can be drawn, but no
spell starts any of them.

## Intended behavior

Each stage of a spell's visual plays at the moment the original plays
it, tied to the server's messages:

- **Stages** (note 6.0): precast from "cast started" until the cast ends
  any way, with the weapon drawn first where the spell needs it and the
  kit's start-then-loop casting animation; cast on "spell go" (not for
  thrown trajectories); instant-area and impact-area kits at a ground
  target; the missile on go; on a hit, the wound animation (or death),
  then caster-impact or target-impact falling back to impact, and the
  state kit if the aura is now on; for spells without a missile, the
  impact kits on each target at go; lingering state from aura applied,
  waiting for a missile still in flight or for the model; "state done"
  once when the aura goes; the channel kit; arrows and bolts stuck in
  the target for 5 s.
- **Where a missile starts** (note 5.5): the caster's `$CSL`, then
  `$CSR`, then `$CST` model event, else a passenger's model position, else
  the unit's position.
- **How it flies** (note 6.1): a straight line at the spell's speed,
  re-aimed every frame at the aim point, leading a moving target as if
  it moved at half speed; the aim point from the visual's attachment
  through the sixteen-entry table (or used directly with flag 512), plus
  the impact offset; attachments 15 then 19 when missing, else 0.75 of
  the target's height; arrival when this frame's step reaches it; a
  missile that will miss aims half a second past, up to 3° off, flies
  on and fades; a vanished target bounces or arrives at once;
  server-timed throws under gravity to the server's arrival time;
  ground-following missiles; a fizzle fading over 0.5 s; the error cube
  for a missing model; item and ammunition models for missile numbers
  −1 to −5.

- **Sounds** (note 6.3, filled 2026-10-16): each kit's own sound starts
  when the kit is ready (its model loaded and attached), at the kit's
  target or else its source, none during cinematic music; one-off kits'
  sounds follow the object; lasting kits loop until they end, and on
  the camera's own character (listener at the character) play
  unplaced at 0.65 volume; priority 110 for the player's own unit or
  the camera's. The missile's flight sound fades in over 0.5 s, loops,
  follows it, and fades out over 0.15 s. The animation-event sound
  plays 1 yard above a player at the cast animation's `$TRD` event, and
  on a melee swing with no target. Ranged attacks borrow any missing
  kit, missile or sound from the ranged weapon's own spell visual, so
  different bows can sound different. Below `violenceLevel` 2, a
  spell's low-violence visual replaces its own.

## Suggested implementation steps

1. The stage machine per cast, driven by the server's cast messages.
2. Missile flight with leading, missing, bouncing, thrown and
   ground-following kinds.
3. The impact, state and channel kits on units.
4. Tests (window-free, with scripted cast messages):
   - precast starts on "cast started" and stops on an interrupt;
   - a missile at 20 yards a second reaches a still target 20 yards
     away in about one second;
   - against a target running across its path it arrives sooner than
     aiming straight at where the target was;
   - a target model lacking the named attachment is hit at attachment
     15, then 19;
   - the state kit waits for a missile still in flight;
   - a missile's flight sound starts silent and is at full volume
     0.5 s later;
   - a bow whose item display names a spell visual lends its missile
     to an auto-shot whose own visual has none.

## Open questions

None of its own. Which kits count as one-off and which as lasting
waits on docs/057 (not traced stage by stage).

## Related

- [docs/057](../docs/057-what-the-original-program-decides.md) notes
  5.5, 6.0, 6.1; [804](./804-floating-combat-text.md).
