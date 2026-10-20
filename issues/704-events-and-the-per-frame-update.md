# 704 — Events and the Per-frame Update

*Written 2026-10-16 from the clean-room checklist's sections 1 and 11
([docs/057](../docs/057-what-the-original-program-decides.md)), where
Phase 7's capstone sketched it as "event system (RegisterEvent / OnEvent
dispatch)".*

## Blocked by

- [703](./703-frames-anchors-and-layers.md) (frames to deliver to and
  update).

## Blocks

- [705](./705-interface-functions.md), [813](./813-auto-loot.md),
  [707](./707-saved-variables.md), [708](./708-phase-demo.md),
  [709](./709-taint-and-combat-lockdown.md),
  [809](./809-targeting-and-range.md).

## Current behavior

Nothing of the interface exists, so no event is fired and no update
script runs. The clock that will hand the interface its turn is designed
([110g](./110g-the-clock.md)) and the interface's place among the frame's
work is written into [110e](./110e-per-frame-work-as-stations.md).

## Intended behavior

Addons hear events, and their per-frame scripts run, in the order the
original gives them. The order is behavior: an addon that reads a unit's
health inside "target changed" sees what the original would show.

- **When an event fires** (row "events in the same frame fire in this
  order", note 11.1): the moment the program does the thing that causes
  it, never later from a queue. So events follow their causes: input
  first, then each packet in arrival order, each packet's events in the
  order its contents are read.
- **Unit events** (note 1.11, filled 2026-10-16): each of a unit's data
  fields from the server has an event; a change fires it at once, while
  the update message is read, **once for every unit token that names
  the unit** (`target`, `party1`, `focus`, ...), so one change can fire
  the same event several times with different tokens. Some unit events
  are **held back** instead: collected through the frame, then sent
  during the per-frame work at most once per unit and event, in
  first-collected order, so they lag up to a frame and repeats merge.
  Spellcast events carry token, spell name, rank text and a cast
  counter; bag and slot events give equipped slots as one number, the
  backpack as 0 and a slot, the bank −1, the keyring −2, the currency
  list −4. The field-to-event pairs and every event's argument kinds are
  in docs/057 Appendix D, with when-it-fires lines for the events
  traced by hand (zone changes and their indoor and new-area forms,
  corpse in range at 40 yards or in an instance, camping against
  quitting, mail success against failure).
- **Events the original never fires** (Appendix D): some names are
  registered but never sent. Earning or losing a title posts a system
  chat line instead of its event, and the GM player-info message is
  read and ignored. These names exist here too and are never fired.
  Others weren't found anywhere in the decompilation, which covers
  about 90% of the original; they are registered and wait.
- **One quirk** (Appendix D, filled 2026-10-16), with a correction
  switch off by default: `UNIT_SPELLCAST_INTERRUPTIBLE` and its
  `NOT_INTERRUPTIBLE` partner fire only for the target, the focus and
  arena opponents, never for the player's own casts or any other
  unit's. The correction checks every unit that has a token.
- **Who hears it, in what order** (note 1.5): oldest registration first;
  registering during that event's delivery takes effect after it;
  unregistering skips the rest of that delivery; registering twice does
  nothing; unregister then register in one delivery cancels the
  unregister; unknown event names fail quietly; an event fired inside its
  own delivery runs to the end first, by the same rules.
- **The per-frame update** (note 1.4): finish deleted frames; anchors
  (703); every shown frame's update script, strata then level then shown
  order, each frame's animations in the same visit; anchors again; then
  the pointer's frame worked out again if anything moved, which can fire
  enter and leave in the same frame. A frame shown during the scripts
  runs its first update next frame.
- **Elapsed time** handed to update scripts: real seconds since the last
  update. While the window is minimised, update scripts are held and the
  time builds up into one lump for the first update afterward, as in the
  original (a **quirk**). Its correction switch, off by default, keeps
  them turning with the world instead ([110g](./110g-the-clock.md),
  answer 6).
- **Going idle** (note 1.8): the time since the last real key, mouse
  button, mouse movement or wheel, checked each frame. After 5 minutes
  the character sits (standing, out of combat, unmounted, not following)
  and is marked away. After 30 minutes the interface's `IDLE_MESSAGE`
  goes to chat and the character logs out, unless a logout is already
  under way. Script calls never reset this clock
  ([709](./709-taint-and-combat-lockdown.md): there is no other
  real-input test).
- **What scripts see** (note 11.1, 110e): units and camera as last frame
  drew them, plus this frame's movement.

## Suggested implementation steps

1. Registration lists per event, with the pending-add and pending-remove
   marks the delivery rules need (note 1.5).
2. Firing at the cause, from the packet handlers (Phase 5) and input,
   with nested delivery.
3. The update pass in note 1.4's order, run as the interface's work on
   the clock (110e, 110g).
4. Tests, each answering a docs/057 note:
   - three frames registered A, B, C hear in that order;
   - B unregistering inside A's handler isn't called this time;
   - D registering inside A's handler isn't called this time, and is
     next time;
   - registering twice delivers once;
   - an event fired from its own handler finishes the inner delivery
     before the outer goes on;
   - a frame shown inside an update script gets its first update next
     frame;
   - a frame moved by an update script fires enter under a still pointer
     in the same frame.

   - a health change on a unit that is both `target` and `party1` fires
     `UNIT_HEALTH` twice, once with each token;
   - a held-back unit event changed three times in one frame is sent
     once, during the per-frame work;
   - five minutes with no input sits a standing character and marks it
     away; one key press before that resets the count;
   - correction off: minimised for 10 s, the first update afterward
     gets about 10 s; correction on: no update gets more than about
     50 ms.

## Open questions

None.

## Related

- [docs/006](../docs/006-ui-and-addons.md),
  [docs/057](../docs/057-what-the-original-program-decides.md) notes 1.4,
  1.5 and 11.1, [703](./703-frames-anchors-and-layers.md),
  [110e](./110e-per-frame-work-as-stations.md), [110g](./110g-the-clock.md).
