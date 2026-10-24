# 814 — Box Select in the WoW World

*Moved here 2026-10-23 from wow-chat-2026, where it was written on
2026-10-04 as issue 617s7 (part of that server's "compass formation" of
companion bots). The owner, then: "Then, ideally, an issue file in the
custom client to allow us to select them by clicking and dragging a
box." And on 2026-10-23, asked whether it belonged here: "let's move it
then."*

## Blocked by

- [809](./809-targeting-and-range.md) (a unit picked from the screen;
  "target changed").
- Phase 5 (units from the server, with their positions).

## Blocks

Nothing yet.

## Related

- 1103, the selection model planned for WC3 map mode (box, shift,
  groups, double-click; sketched in [1108](./1108-phase-demo.md), no
  file yet): the same machinery. Whichever is built first, the other
  reuses it.

## Current behavior

Nothing exists. The original 3.3.5a client selects one unit by clicking
it: one target at a time, and the server is only ever told which single
unit is targeted. It has no box selection, and its addons cannot draw
one over the 3D world (they can only draw on the interface layer, and
cannot ask which units are under a rectangle of the screen).

## Intended behavior

In the WoW world, the player clicks and drags a box over the world;
every one of the player's own companions inside it is selected
(highlighted), and the next order applies to all of them.

- **This is not the original's behaviour.** It belongs to the separate,
  optional layer for one server's own features, built after the
  original is rebuilt (the owner, 2026-10-06: the client is first
  "something that anyone can use to play World of Warcraft private
  servers").
- **The server it is for:** wow-chat-2026 (Everland Ghostsong), whose
  players lead companion bots ("buddies"). There, buddies are ordered by
  chat addressing: `@party`, `@<name>`, `@<n>` (the n-th member of the
  party), several at once as `@1,3` or `@Pozeta,Muspo`, then a click on
  the ground to send them there (wow-chat-2026 issue 617s6). A box
  selection is a quicker way to write that address: the client turns
  the selected units into the same addressing and sends it, so the
  server needs no change.
- **Which units can be selected:** only the player's own companions
  (members of the player's party the server marks as theirs), never
  other players or monsters; selecting does not change the ordinary
  target.

## Suggested implementation steps

1. A drag on the world (with the button the camera's drag does not use,
   or a modifier: see open questions) draws a rectangle on the screen.
2. On release, each candidate unit's position is projected to the
   screen (the same projection the renderer uses); those inside the
   rectangle are the selection. Shift adds to it.
3. The selection is highlighted (a ring under each unit, as in WC3 map
   mode).
4. The next order is sent as the server's own addressing (for
   wow-chat-2026, `@<n>,<n>` followed by the click), so the server sees
   only what a player could have typed.

## Open questions

1. **Which drag:** in the original, a left drag orbits the camera and a
   right drag turns the character. Box select needs a third gesture: a
   modifier held with the left drag, a dedicated mode (as WC3 map mode
   has), or a key binding that switches the left drag between orbit and
   select?
2. **How the client knows the server's addressing:** written for
   wow-chat-2026 only, or a small per-server setting (each server is a
   profile of the client's own, with its own settings: wow-chat-2026
   issue 173v, answer 9) that says how a selection is turned into an
   order?
