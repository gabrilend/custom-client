# 709 — Taint, Protected Functions and Combat Lockdown

*Written 2026-10-16 from the clean-room checklist's note 1.7 and the
row "these actions require a real key press or click"
([docs/057](../docs/057-what-the-original-program-decides.md)). Phase 7's
roadmap line calls this "sandboxing".*

## Blocked by

- [704](./704-events-and-the-per-frame-update.md) (each fresh entry into
  the scripts is where taint resets; the feedback events).

## Blocks

- [708](./708-phase-demo.md).

The XML reader's rule that a template loads with its definer's taint is
switched on once this exists; the XML reader doesn't wait for it.

## Current behavior

Nothing exists: no scripts run, so nothing is trusted or tainted.

## Intended behavior

Addons can do exactly what they could in the original and are refused
exactly where they were, with the same events, so addons that work
around the rules (secure templates, out-of-combat queues) behave the
same.

- **How taint spreads** (note 1.7, said as behavior): every value carries
  an empty (trusted) or addon-named label; running code has a current
  taint; values made or stored take it; trusted code reading a tainted
  value becomes tainted for the rest of its run; calling an addon's
  function runs with its creator's taint; after a built-in call a
  tainted function gets its own taint back; each fresh entry (event,
  frame script, key binding, slash command) starts from its handler's
  taint; a few runs are made trusted on purpose (saved variables, the
  attribute-changed script, error handling, table re-sizing); macros
  carry a taint of their own; an optional reporting hook.
- **Trusted code:** the default interface, and the signed "Blizzard_"
  addons loaded on demand.
- **No real-key-press test.** Protection is by taint only; trusted code
  may call protected functions at any time. Real input only stamps
  movement and camera actions with its time and drives going idle
  ([704](./704-events-and-the-per-frame-update.md)).
- **Protected functions, by category** (note 1.7): always refused while
  tainted (movement, turning, jumping, steering, auto-run, combat mode,
  spell casting and targeting, macros, loot method, tab and direction
  targeting, focus), firing `ADDON_ACTION_FORBIDDEN` or
  `MACRO_ACTION_FORBIDDEN`; refused but reported as blocked (trade,
  auction, LFG, calendar, battlefield port, and some cases of reload,
  cancel-logout and uninvite), firing `ADDON_ACTION_BLOCKED` or
  `MACRO_ACTION_BLOCKED`; refused only in combat lockdown (raid moves,
  macro editing, key bindings, picking up and placing actions, action
  bar pages, equipping); never refused (`SendChatMessage`). One feedback
  event per refusal, even nested.
- **Protected frames** (note 1.15, filled 2026-10-16): a region counts
  as protected when it was marked so (in XML, or as a secure template),
  or when a child of it is protected, or something protected is
  anchored to it. Its protected methods are refused only when all three
  hold: protected, tainted caller, combat lockdown; the refusal fires
  `ADDON_ACTION_BLOCKED` and comes before the argument check, so a
  wrongly called method is refused silently
  ([703](./703-frames-anchors-and-layers.md)).
- **Combat lockdown:** starts when the server marks the player in combat
  (unit flag 524288), **after** `PLAYER_REGEN_DISABLED` fires, so its
  handlers can still act; ends **before** `PLAYER_REGEN_ENABLED`; starts
  the same way on entering the world in combat; lifted, with its event,
  before `PLAYER_LOGOUT`.

## Suggested implementation steps

1. Where the labels live (answer 2 below): LuaJIT unchanged. A
   variable the scripts share (a global, a field of a table) is reached
   through a table, and beside it sits a separate table of modifiers,
   keyed the same way, holding that slot's taint label (who last wrote
   it). Writing a slot sets its label to the running code's taint;
   reading a tainted slot from trusted code taints the running code. The
   running code's current taint is one value the host keeps and resets
   at each fresh entry into the scripts. Functions an addon makes are
   remembered in the same kind of side table, with their creator's
   taint.
2. The spreading rules, the resets, the exempt runs, the macro taint,
   the hook.
3. The table of protected functions by category, and the two pairs of
   feedback events.
4. Combat lockdown from the server's unit flag, with its event order.
5. Tests, each answering note 1.7:
   - an addon calling `MoveForwardStart` is refused and fires
     `ADDON_ACTION_FORBIDDEN` naming it and `MoveForwardStart()`;
   - default-interface code reading an addon's global, then calling a
     protected function, is refused;
   - the same code in the next event is trusted again;
   - `PLAYER_REGEN_DISABLED`'s handler can still change a key binding;
     one second later it can't;
   - a refusal nested in another fires one event, not two.

## Open questions

1. **The "hook after" helper.** Addons usually extend a default
   interface function by having their own function run *after* it,
   rather than replacing it. The helper that does this puts the
   caller's clean state back afterwards, so the default interface isn't
   left tainted (from public addon-writing knowledge; docs/057 doesn't
   cover it yet). Replacing the function instead leaves the interface
   tainted until the event ends, which is what broke action bars in
   combat. In this client's design, each place a value is kept has a
   label in a side table, and running code has one current label kept
   by the host. What would the helper need to save before it calls the
   addon's function, and where would it put it back? Asked 2026-10-16;
   the owner: "I'm not sure, let's leave it as an open question for now
   because I'll need to decide sometime." Asked again 2026-10-20: "um,
   decide later I think."

## Answered

2. ~~**Carrying taint in LuaJIT.**~~ Not by changing LuaJIT (2026-10-16).
   Offered a modified LuaJIT, the reference Lua, or separate
   environments, the owner: "Modify LuaJIT? Get real! Just have the
   variable be a table, and have a separate spot for any variable
   modifiers." So the label belongs to the place a value is kept, held
   in a side table beside it, as step 1 describes. A test to write
   early: a value an addon wrote, copied by trusted code into a local
   and then stored somewhere else, must arrive tainted. Note 1.7 says
   the copy carries the taint, and a local isn't a slot with a side
   table, so the running code's own taint has to carry it across.

## Related

- [docs/006](../docs/006-ui-and-addons.md),
  [docs/057](../docs/057-what-the-original-program-decides.md) note 1.7,
  [704](./704-events-and-the-per-frame-update.md),
  [809](./809-targeting-and-range.md) (tab targeting refused to addons).
