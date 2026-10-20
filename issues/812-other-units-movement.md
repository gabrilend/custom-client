# 812 — Other Units' Movement

*Written 2026-10-16 from the clean-room checklist's note 2.5
([docs/057](../docs/057-what-the-original-program-decides.md)). The
server sends where other players and creatures are, a few times a
second; the original decides how they are shown moving between those
messages.*

## Blocked by

- [801](./801-movement-by-the-servers-rules.md) (the movement state and
  its messages), Phase 5 (receiving them).

## Blocks

- [808](./808-phase-demo.md).

## Current behavior

Nothing of this exists. The world's creatures stand still where a list
generated from a world database puts them
([404](./404-characters-and-equipment.md)).

## Intended behavior

Other units move smoothly and a little in the past, as in the original
(note 2.5):

- **A delay queue per unit:** a movement message is applied at its
  server time stamp, turned into local time, plus a delay. The delay is
  the largest of the unit's last 32 measurements of how late messages
  have been (each kept within ±32.768 s), starting at 50 ms.
- **Blending:** while an update waits, the unit moves smoothly toward
  it: position if moving, facing if different, pitch when swimming or
  flying. No blend across a teleport or a change of transport.
- **Guessing ahead** with nothing queued: full speed for 0.5 s after the
  last update, slowing in a straight line to a stop by 1.5 s, so never
  more than 1 s of travel past it. Any new message resets this.
- **Standing:** other units may stand on slopes up to 80°
  ([802](./802-collision.md)).
- **The player's own unit** never goes through any of this.

## Suggested implementation steps

1. The per-unit queue and its 32 lateness measurements.
2. Blending toward the waiting update; the teleport and transport
   exceptions.
3. Guessing ahead and stopping.
4. Tests (window-free, with scripted messages and a scripted clock):
   - with messages always 80 ms late, the delay settles at 80 ms;
   - one message 300 ms late raises the delay to 300 ms until it falls
     out of the last 32;
   - a unit running at 7 yards a second with no news for 3 s has gone
     7 yards (3.5 in the first half second at full speed, 3.5 more while
     slowing to a stop by 1.5 s) and then stands;
   - a teleport moves the unit at once, with no slide.

## Open questions

None.

## Related

- [docs/057](../docs/057-what-the-original-program-decides.md) note 2.5,
  [801](./801-movement-by-the-servers-rules.md).
