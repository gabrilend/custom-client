# 801 — Movement by the Server's Rules

*Came from: wow-chat-2026 173a (first light), 173i (walking), 173r
(swimming) and 173e (what the server checks), moved 2026-10-07.*

## Current behavior

Built (window-free, tested alone):
- **The ground the server uses** (`src/018-maps.c`): the server's own map
  files, with the server's own arithmetic for height and steepness, so
  she stands where the server thinks she stands. Shared by two threads
  under a lock in the world it belongs to.
- **The slope rule** (`src/019-navmesh-slope.c`): the steepest walkable
  slope, from the server's navigation tiles.
- **One step of movement** (`src/029-mover.c`): the game's own speeds
  times her bonus (run 7, back 4.5, walk 2.5, swim 4.72 yards a second),
  turning, strafing, jumping, falling, sliding down ground too steep;
  swimming in deep water (up with the jump key, down with X, forward
  following the camera's pitch), walking out onto a shore. Floors and
  walls from the models' shapes (802). The speed the keys ask for, for
  the animation's pace.
- **Controls:** W/S, A/D (strafing with the right button), Q/E, Space,
  the keypad's / to walk; both buttons run forward.
- Tests: a second forward is 8.4 yards; back 5.4; walking 3 every way;
  jumps rise and land; slopes and cliffs; swimming and floating.

Measured against the clean-room checklist
([docs/057](../docs/057-what-the-original-program-decides.md), section 2,
filled 2026-10-16):
- **Already the original's:** gravity (19.291 yards a second squared),
  the jump's take-off speed on land (7.956), the fastest fall (60.148),
  the step up (1 yard).
- **Not yet the original's:**
  - when she starts and stops swimming: this client uses a fixed 1.6
    yards of water over her feet; the original uses 0.75 of her body
    height, with a slightly lower depth to stop (note 2.3);
  - a jump while swimming (9.097 a second), slow-fall's fastest fall
    (7.0), and "falling far" (notes 2.1);
  - speed and direction held from take-off until landing, the walking
    drift of a jump from standing, and keys pressed in the air applied
    on landing (note 2.1). This client steers freely in the air;
  - turning at 75% while moving or falling (row "turning speed");
  - which speed applies to backward, walking, diagonals and strafing
    while pitched, and turning while moving following a true arc
    (note 2.4);
  - the 45° rise and dive of swimming, climbing out near the surface,
    and the wading splash (note 2.3);
  - a long frame: the original moves at most 250 milliseconds at a time
    and skips the rest, telling the server of the skip (note 2.1);
  - knockbacks (row "a knockback flies on this curve").
- **Close to the original:** sliding. The original has no slide speed of
  its own: on ground steeper than 50° she can't stand, so she falls under
  ordinary gravity, pushed along the surface (row "the steepest walkable
  slope"). This client pushes her along the slope by gravity too.
- **Not yet built:**
  - telling the server what she does (Phase 5): a heartbeat every 500
    milliseconds while moving (not while only turning); one message at
    the moment of each start, stop, turn, pitch, jump, run/walk switch,
    facing and swim change; a heartbeat (not a jump) on stepping off an
    edge; "fall land" only after a fall of more than 1/36 yard; the fall
    time in whole milliseconds; a jump packet on leaving the water; an
    acknowledgement, carrying the server's counter, for each change the
    server forces (row "these changes send a movement packet at once");
  - boats, zeppelins and lifts (note 2.6): aboard whatever transport owns
    the ground under her, with no "board" action; staying aboard while
    inside the transport's bounds, even in a jump; position and facing
    kept relative to the vessel; transports moved before any unit;
  - auto-run (note 8.8, filled 2026-10-16): one switch, refused to
    tainted code, cancelling a channelled spell when turned on; the
    forward push is auto-run (sets it to 1) + forward key + both mouse
    buttons − backward key, its sign deciding forward, back or stop;
    turned off by the forward or backward key, by the second mouse
    button joining the first, by any moment movement isn't allowed, or
    by the switch; **not** by leaving the game window (which releases
    held keys) or by strafing and turning; a unit marked unstoppable,
    or on an unstoppable vehicle, always moves forward. This client's
    controls have "both buttons run forward" but no auto-run switch;
  - click-to-move and following (note 8.6): walking to a point or a unit
    and stopping within each action's distance, turning on the way at
    half a turn a second.

## Intended behavior

She moves exactly as the server expects (wow-chat-2026 173e): the same
heights, speeds, slope limit and falls, so a server with its checks on
never corrects her.

## Suggested implementation steps

Each step builds from a row or note of
[docs/057](../docs/057-what-the-original-program-decides.md) section 2,
and links back to it when done:

1. Swimming by body height (note 2.3): start above 0.75 of her height,
   stop below that less 1/36 yard; float with the top quarter of her
   body above the surface; rise and dive at 45°; climb out within 1/3
   yard of the surface; the jump out of water at 9.097. Her body height
   comes from 802's body.
2. The air (note 2.1): speed and direction fixed at take-off; keys
   pressed in the air kept for landing; a standing jump drifts at walk
   speed; roots held until landing; "falling far" after 1/9 yard below a
   jump's start or 500 ms of any other fall; slow-fall's 7.0.
3. Speeds and turning (note 2.4, row "turning speed"): the backward and
   walking rules, diagonals at 0.7071 each way, strafing always level,
   turning at 75% while moving, paths along arcs.
4. Long frames: at most 250 ms moved at once, the rest skipped and
   counted for the server.
5. Knockbacks on the fall curve.
6. The movement packets (Phase 5), checked against the original's bytes
   (wow-chat-2026 173l's tap): the 500 ms heartbeat, fall time, every
   message that goes at once and its exceptions, the acknowledgements.
7. Transports (note 2.6): the owner of the ground under her decides
   which transport she's on; coordinates relative to it; the in-bounds
   exception; converting on boarding and leaving so she doesn't jump.
   Needs game objects from the server ([802](./802-collision.md) step 3).
8. Click-to-move and following (note 8.6), with each action's stopping
   distance and starting limit; `AUTOFOLLOW_END` when a new click
   cancels a follow.
9. Tests for each: a jump's peak after about 0.412 s; terminal speed
   after about 3.118 s; turning in the air changes facing but not path;
   she swims in water 0.75 of her height deep and not in shallower;
   stepping off a ledge 0.02 yard high sends no "fall land"; jumping on a
   moving deck lands her on the deck; following a unit 31 yards away is
   refused; pressing backward while auto-running switches auto-run off
   and moves her back; auto-run with both mouse buttons pressed in turn
   switches off on the second button; switching windows leaves
   auto-run on.

## Open questions

None left of the first two: her step height (1 yard, times her size)
and swim depth (0.75 of her body height) are answered in docs/057
section 2 (2026-10-16), and her body is 802's.

## Related

- [802](./802-collision.md), [309](./309-water.md), [402](./402-skeletons-and-animation.md).
- **Blocks:** [809](./809-targeting-and-range.md) (positions and facing for targeting), [812](./812-other-units-movement.md).
