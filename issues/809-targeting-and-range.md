# 809 — Tab Targeting and the Range Check

*Written 2026-10-16 from the clean-room checklist's section 8
([docs/057](../docs/057-what-the-original-program-decides.md)). These are
choices the original makes on the player's side: the server never sees
how the player picked a target, only which one.*

## Blocked by

- [801](./801-movement-by-the-servers-rules.md) (positions and facing),
  Phase 5 (units from the server),
  [704](./704-events-and-the-per-frame-update.md) (the interface asks for
  range; targeting fires "target changed").

## Blocks

- [808](./808-phase-demo.md) ("target NPC, ... walk to mob, attack").

## Current behavior

Nothing exists: there are no units from the server yet, no target, and
no spells.

## Intended behavior

The player's target is picked, and action buttons turn red for range,
exactly as in the original, so tab and the action bar feel the same in
the hand.

- **Tab targeting** (row "tab targeting", note 8.1): within 41 yards in a
  60° cone ahead, or 10 yards all round; units ahead first, then
  nearest; distance in three dimensions. One shared search for seven
  kinds (nearest enemy, enemy, enemy player, friend, friend player,
  party, raid), each with who counts and who is left out. A sorted list
  stepped through on each press and backward with shift; rebuilt after
  3 s, on a change of kind, or on wrapping when over 1 s old; a target
  clicked by hand makes the next press retarget the list's current entry
  rather than step. Called from addon code: nothing, and the "blocked
  action" feedback.
- **Range** (row "action buttons turn red", note 8.2): centre to centre
  in three dimensions; friendly or hostile range by who the target is;
  melee reach (both combat reaches + 4/3 yard, at least 5); ranged dead
  zones; 8/3 yard leeway when both are moving fast; the ranged weapon's
  percent; talents and auras flat then percent; "unlimited" as 100
  yards; "no answer" rather than "out of range" for invalid targets. The
  interface asks with `IsActionInRange`: 1, 0, or nothing.
- **Nameplates** (note 8.3): shown within 41 yards outside arenas.
- **Under the mouse** (row "the unit under the mouse", note 8.4): a
  nameplate's unit wins first. Otherwise a line from the camera through
  the pointer is cut short at the first terrain or building, then tested
  against each highlightable model's **real triangles as posed this
  frame**, attachments included, skipping extra material layers and
  pieces invisible right now. The nearest hit wins. If nothing is hit, a
  second pass ranks by group: the object already under the mouse, living
  units, lootable corpses, usable game objects, the rest; nearest within
  a group. Low-priority units only when nothing else was hit.
- **Reaction and colours** (row "a unit's name and circle colour", note
  8.5): reaction on the eight-step scale from hated to exalted, worked
  out in order (itself; two player-controlled units: duels, same
  controller, party members, free-for-all; reputation, where the
  player's standing gives the level; else the faction-template table's
  enemy and friend rules, neutral when missing). Selection colours from
  reaction for creatures, and from who can attack whom for
  player-controlled units, with a second variant adding party, friend
  and dead colours; units out of sight coloured by the party list's PvP
  flag.
- **Who can attack or help whom** (note 8.7, filled 2026-10-16), the
  rules behind `UnitCanAttack`, `UnitCanAssist`, `UnitIsFriend`
  ([705](./705-interface-functions.md)) and the red, blue and yellow
  colour choice:
  - *attack*, checked in order: a barred attacker; ghosts (only
    ghost-fighting creatures); targets that can't be attacked at all
    (five unit flags); immunity to the other's kind, both ways; a
    vehicle and its own riders; two non-player-controlled units fight
    when either hates the other; two player-controlled units by duel,
    PvP flag, free-for-all and sanctuary; one of each when the other
    isn't in a sanctuary and the attacker isn't friendly;
  - *attack right now*, a stricter form: not while dead or a ghost (but
    for the player possessing another unit), not mounted (with three
    exceptions), not against health 0 unless feigning;
  - *help*: not an unselectable target; immunity one way; friendly
    reaction (or the everyone-friendly creature flag); duel sides,
    free-for-all and sanctuary for player-controlled targets; a player
    helps a creature only when it's flagged for PvP or marked as one
    players may help;
  - *cooperate* (the program's own group checks): not the same unit,
    neither charmed, same faction group, and not able to attack.

  Each flag is the value the server sends, so the tests can be built
  from AzerothCore's own definitions.

## Suggested implementation steps

1. The search over units in view, the seven kinds' rules, the cone and
   the all-round circle, the ordering.
2. The list's lifetime (3 s, kind change, wrap after 1 s) and stepping.
3. The range answer with every rule of note 8.2, as one function the
   action bar and the interface both ask.
4. Nameplate distance.
5. The mouse-over test against posed triangles (needs
   [402](./402-skeletons-and-animation.md)'s poses on this side of the
   frame, not only on the graphics card), its two passes and the sticky
   highlight.
6. Reaction and the two colour variants as tables; the attack, attack
   right now, help and cooperate rules (note 8.7) as one ordered list
   each. Tests: a ghost can't be attacked except by a ghost-fighting
   creature; two PvP-flagged players can attack each other but not in a
   sanctuary; a player can't help an unflagged hostile creature; a unit
   feigning death at health 0 can still be attacked.
7. Tests, each answering a docs/057 note:
   - the pointer over a unit standing behind a wall picks nothing;
   - the pointer between a unit's raised arm and its body picks
     nothing, where a box test would pick it;
   - a revered faction's guard counts as revered; two players dueling
     each other are hostile;
   - a player who can attack you but you can't attack is blue;
   - a unit 40 yards dead ahead is picked before one 9 yards behind; one
     11 yards behind isn't considered;
   - three presses step three units; a fourth press 1.5 s after the
     list was built wraps to the nearest again;
   - melee range against two units with combat reach 1.5 is 5 yards (the
     floor), and with reach 2 each is 5.333;
   - an invalid target gives "no answer", not 0.

## Open questions

None for the owner. Which of the original's two colour variants it uses
where is the describer's to find (answered question 1); docs/057 note
8.5 says it wasn't checked.

## Answered

1. ~~**Which colour variant where.**~~ The plainer variant until docs/057
   says otherwise, the fuller one behind a switch (2026-10-20). Both
   variants are the original game's own: the second adds party, friend
   and dead colours, and note 8.5 says which one the original uses
   where wasn't checked. The owner, reading the second as new colours:
   "it'd be pretty neat if we had more colors, but obviously they
   should be not included in the default functionality. This is an
   example of a usecase for the 'build it original, then patch the cool
   new behavior on top' pattern." So:
   - **Default:** the first variant everywhere, the only one the
     project can't be wrong to call plain.
   - **The switch** (the Names and nameplates panel,
     [1001](./1001-the-options-menu.md)): the second variant everywhere
     it fits, off by default.
   - **When docs/057 answers:** the default changes to whatever the
     original actually does in each place, even if that's the second
     variant somewhere, and the switch keeps "the fuller colours
     everywhere". Original first wins over "fewer colours".
   - **Tests:** a party member's circle uses the first variant's colour
     with the switch off and the party colour with it on.

## Related

- [docs/057](../docs/057-what-the-original-program-decides.md) section 8,
  [docs/006](../docs/006-ui-and-addons.md), [801](./801-movement-by-the-servers-rules.md).
