# 804 — Floating Combat Text over Units

*Written 2026-10-16 from the clean-room checklist's note 6.2
([docs/057](../docs/057-what-the-original-program-decides.md)), at the
number Phase 8's capstone sketched for "combat log + floating combat
text". The scrolling text around the player's own character is the
interface's combat-text addon, which is data the default interface
already carries; this issue is the other kind, the numbers drawn over
units in the world by the program itself.*

## Blocked by

- [206](./206-bitmap-text.md) (text and its shadow), Phase 5 (the combat
  messages).

## Blocks

- [808](./808-phase-demo.md).

## Current behavior

Nothing exists. No combat happens yet.

## Intended behavior

Damage, healing, misses, experience, honour and threat text rise, grow
and fade over units exactly as in the original (note 6.2):

- **Each kind's numbers** from the note's table: how far it rises, its
  fade-in, when its fade-out starts, when it's gone, its height as a
  fraction of the screen, its colour.
- **The critical pop:** grows from 0.1 to 2.0 × its height over the
  first tenth of its life, shrinks back to 1.0 × by the fifth, then
  holds.
- **Fading** in from clear, opaque between, out in a straight line.
  A black shadow offset 0.002 of the screen, half opaque, fading along.
- **Placement** at the unit's position plus the rise, carried with a
  camera on a transport, centred and kept on screen, overlapping texts
  pushed apart, nothing drawn behind the camera.
- **Who shows what:** healing only for the player's (or pet's) heals on
  others with `CombatHealing` on; threat only with threat warnings and
  `threatWorldText` on, using the interface's threat strings, a new one
  replacing the old; misses in the interface's `COMBAT_TEXT_…` strings.

### Quirk, with a correction switch (off by default: the original)

| Quirk | The original | The correction |
|---|---|---|
| "absorb" text (6.2) | its fade-out starts at 90 ms, before its 150 ms fade-in ends, so it's barely seen at full strength and fades over 1.4 s | start its fade-out at 760 ms like plain damage |

## Suggested implementation steps

1. The kinds as one table of numbers (note 6.2).
2. The life of one text: rise, size curve, fade, shadow.
3. Placement and pushing apart in screen space.
4. The "who shows what" rules and the interface's strings.
5. Tests (window-free, measuring a text's state at set moments):
   - a damage text is fully opaque at 500 ms and gone at 1500 ms;
   - a critical text is 2.0 × its height at a tenth of its life;
   - "absorb" is never fully opaque, and is with the correction on;
   - two texts on one unit at once don't overlap on screen.

## Open questions

None.

## Related

- [docs/057](../docs/057-what-the-original-program-decides.md) note 6.2,
  [811](./811-spell-visuals.md).
