# 110c — The Mailbox

*Parent: [110](./110-the-ceramic-shape.md).*

## Current behavior

*Paused with its parent, 2026-10-07: blocked until soramech has timers
and standing tasks ([110](./110-the-ceramic-shape.md)).*

There is nothing between the work and the drawing: the main thread does
both, in turn. world-edit-to-execute built and measured the mailbox
(515c: 725 frames a second uncapped against 472 taking turns) and
extrapolation with snapping corrections (515d).

## Intended behavior

Each view (each render thread) has a **three-buffer mailbox** of what to
draw: one buffer being drawn, one holding the newest complete state, one
being written by stations. Finishing a write swaps it with "newest" in
one atomic exchange; starting a draw does the same. Neither side waits,
a half-written state is never seen, and a newer state replaces an older
waiting one instead of queuing behind it.

A state holds, per thing drawn: which asset (an id into 110d's table),
where, facing, velocity, the animation and the moment it was true. The
render thread draws "now" from it:

- **Extrapolate, don't interpolate**: a unit is carried forward from its
  newest state, capped at twice the ping; past the cap it stays where
  the guess reached, still playing its moving animation in place, so
  the player can see it is lag (owner, 2026-09-25, 515).
- **Corrections snap** in the frame they arrive, position and animation
  moment both; nothing is blended (the swing whose target was already
  dead is cancelled mid-air).

## Suggested implementation steps

1. Port 515c's mailbox and its tests (no half-written state seen under
   heavy alternation; `--tsan` clean).
2. The per-thing record, sized for the client's figures (asset id, place,
   facing, velocity, animation id and moment, team colour).
3. Port 515d's extrapolation, cap and snap, with the swing test.

## Acceptance

- The mailbox tests pass under the race-checked build.
- The swing-cancel test passes.

## Related

- world-edit-to-execute 515c, 515d; [110e](./110e-per-frame-work-as-stations.md) (what writes it).
