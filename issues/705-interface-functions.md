# 705 — The Interface Functions

*Written 2026-10-16 from the clean-room checklist's notes 1.12 and 1.13
and its Appendices B, C and D
([docs/057](../docs/057-what-the-original-program-decides.md)), at the
number Phase 7's capstone sketched for "FrameXML API surface
(CreateFrame, Get/SetText, UnitName, …)".*

## Blocked by

- [704](./704-events-and-the-per-frame-update.md) (events),
  [703](./703-frames-anchors-and-layers.md) (widgets).

## Blocks

- [708](./708-phase-demo.md).

## Current behavior

Nothing exists. No interface function is defined.

## Intended behavior

Every function, widget method and event name the original registers
exists here under the same name, so the default interface and addons
find what they look for. A name whose behaviour isn't built yet is
still present, and calling it raises a named error ("<name> isn't built
in this client yet"), never a silent nil: a missing feature shows
itself where it's used.

- **The full list** comes from docs/057's appendices: widget methods by
  widget type with each type's parents (B), interface functions by the
  part of the interface that owns them (C), events with their argument
  kinds (D), each with the original's own usage text. The usage text is
  the error a wrongly called function raises.
- **Unit tokens** (note 1.12): `player`, `vehicle`, `pet`, `target`,
  `partyN` and `partypetN`, `raidN` and `raidpetN`, `bossN`, `arenaN`
  and `arenapetN`, `commentatorN`, `mouseover`, `focus`, and the whole
  tokens `npc`, `questnpc`, `none`; any case; any number of `target`
  and `pet` parts after (each optionally after `-`), with `pet` only
  after the player or a party member; else tried as a character's name
  (player, then raid or party, ignoring case, `-Realm` allowed, cut at
  48 bytes). "Valid but nobody" is kept apart from "invalid".
- **The first unit functions** (note 1.13), each with its answers for a
  known unit, a party member out of sight (from party-status updates),
  and nobody: `UnitExists`, `UnitHealth` (predicted health, held between
  1 and the maximum, dead shows 0), `UnitHealthMax`, `UnitPower` (types
  0 to 6, rage and runic power ÷ 10, happiness ÷ 1000, predicted power),
  `UnitPowerMax`, `UnitPowerType` (with the vehicle variant), `UnitName`
  (name then realm; a cache query and `UNKNOWNOBJECT` while unknown),
  `UnitLevel` (−1 for bosses and hostiles 10 or more above; drunk
  players see attackable units lower), `UnitXP`, `UnitXPMax`,
  `GetMoney`, `GetTime`, `GetGameTime`; `ReadFile`, `DeleteFile` and
  `AppendToFile` always raise "Access Denied".
- **The second batch of unit functions** (note 1.14, filled
  2026-10-16), each answered from the unit's server fields, the party
  status, or the name cache, with "yes" as 1 and "no" as nil unless
  stated:
  - who the unit is: `UnitRace`, `UnitClass` (gendered display name and
    file token), `UnitSex`, `UnitFactionGroup`, `UnitClassification`,
    `UnitCreatureType`, `UnitCreatureFamily`, `UnitGUID` (`0x` and 16
    upper-case hex digits, the spawn counter zeroed for some creatures),
    `UnitIsUnit`;
  - what kind it is: visible, player, corpse, player-controlled,
    charmed, possessed, on a taxi, in a vehicle, has the vehicle
    interface (a true or false, not 1 or nil);
  - life and death: dead (with feigned death for raid members and
    pets), feigning, ghost, dead-or-ghost, connected;
  - combat: in combat, PvP, sanctuary, the three tapped flags, trivial,
    and the two threat functions;
  - relations: `UnitReaction` (1 to 8), `UnitIsEnemy`, `UnitCanAttack`,
    `UnitCanAssist`, `UnitIsFriend`, by the rules in
    [809](./809-targeting-and-range.md) (docs/057 note 8.7);
  - group and status: in party, in raid, player or pet in raid, party
    leader, away (never shown to enemies; the player's own counts at
    once), busy;
  - casting: `UnitCastingInfo` (nine values) and `UnitChannelInfo`
    (eight);
  - auras: `UnitAura`, `UnitBuff`, `UnitDebuff` by index or by name,
    the filter words, the eleven values, the caster's token order, the
    aura order (first seen first, new ones at the end, several arriving
    together in reverse slot order, gaps closed), hidden auras, and the
    party-status form for members out of sight;
  - the player's own numbers: mana and power regeneration, honor and
    arena currency with their caps, character points, whether the
    player can teleport.
- **The action bar** (note 1.16, filled 2026-10-16): 144 slots, each
  empty or holding a spell, item, macro, equipment set or pet action;
  the server keeps slots 1 to 120 and every change goes back to it at
  once, one slot at a time (changes ignored until the server's list has
  arrived); 121 to 132 are the temporary bar the program fills (a
  form's preset spells, an override set, the possess bar), never sent
  and never picked up or placed; 133 to 144 are the totem bar, three
  rows of four by element, auto-filled into empty slots when a first
  spell of an element is learned. The reading functions (`HasAction`,
  `GetActionInfo`, texture, count, cooldown, usable, current,
  auto-repeat, attack, equipped, consumable, stackable, range), picking
  up and placing, the extra `ACTIONBAR_SLOT_CHANGED` cases, pages, the
  bonus bar offset, toggles, and the totem functions.
- **The spell book** (note 1.17): tabs, names and ranks, textures,
  `GetSpellInfo`, links, counts, cooldowns, usable and the yes/no
  questions, range, `IsSpellKnown`, slot lookups, pet spells,
  companions.
- **Money text** (note 7.9, filled 2026-10-16): copper split into gold
  (÷ 10000), silver and copper; `GetCoinText` joins the non-zero parts,
  largest first, with ", " or the given separator, each filled into the
  interface's `GOLD_AMOUNT`, `SILVER_AMOUNT`, `COPPER_AMOUNT`;
  `GetCoinTextureString` joins them with one space using the
  `…_AMOUNT_TEXTURE` texts and a height (default 14), and 0 gives the
  copper form; `GetCoinIcon` picks one of six coin icons by the amount's
  size. Other number formatting is the interface's own Lua.
- The rest of each function's answers are added family by family as
  docs/057 fills them.

### Quirks, each with a correction switch (off by default: the original)

| Quirk (docs/057 1.14) | The original | The correction |
|---|---|---|
| `UnitClassification` | rank 6 gives nil, past 6 an undefined answer | `normal` for any rank without a word |
| `UnitIsUnit` | the same token text twice is "yes" without looking up, so `("target", "target")` is yes with no target | look both up |
| `UnitInRaid` | the raid slot counting from 0, so the first slot reads as 0 | count from 1; expected to be load bearing, since the stock interface may add 1 itself |
| `GetManaRegen` | 0.001 added to each value, so 0 shows as 0.001 | leave it off |
| `PlayerCanTeleport` | doesn't check the player exists; called before entering the world, the original crashes | false ("can't teleport"); answered question 1 |
| `GetActionInfo`, `GetActionText` (1.16) | the slot isn't checked; outside 1 to 144 the original reads past its list, with no defined result | give nothing; answered question 1 |
| `GetActionBarPage` (1.16) | always 1 while the temporary bar is in use (the chosen page is hidden, not lost) | give the chosen page |
| `GetMultiCastTotemSpells` (1.16) | the element wraps: 5 means 1, 0 means 4 | nothing outside 1 to 4 |
| `SetMultiCastSpell` (1.16) | the usage error only when **both** arguments aren't numbers | when either isn't |
| `GetSpellInfo` (1.17) | with no player, or a pet spell with no pet, only seven of its nine values | always nine, with 0, 0 for range |
| `FindSpellBookSlotByID` (1.17) | the slot counted from 0, though every slot-taking function counts from 1 | count from 1 |
| `GetCoinText` (7.9) | an amount of 0 gives empty text | `COPPER_AMOUNT` with 0, as the picture form does |
| `GetCoinTextureString` (7.9) | its usage error names `GetCoinText` by mistake | name the right function |
| `SummonRandomCritter` (1.17) | claims to give one value without supplying one; the caller gets whatever was left over (often its own last argument) | give nothing; answered question 1 |

## Suggested implementation steps

1. A generator that reads Appendices B, C and D from docs/057 (reading
   only) and writes the name tables: every widget type with its parents
   and methods, every function, every event with its arguments, each
   with its usage text. It prints how many of each it found, so the
   counts are never copied into documents by hand. It reads Appendix D's
   when-it-fires lines too, including the ones the describer added by
   hand, and marks the events the original never fires.
2. Every generated name bound to the "isn't built yet" error until its
   behaviour exists; a list of what's built, so the generator never
   overwrites a real one.
3. The unit-token reader (note 1.12).
4. The first unit functions (note 1.13), and `IsActionInRange` from
   [809](./809-targeting-and-range.md).
5. Tests:
   - `party2targettarget`, `raid5-target` and `pettarget` read; 
     `targetpet` names nobody; `targetfoo` is invalid;
   - `UnitHealth("target")` with no target is 0, and `UnitName` gives
     nil, nil;
   - a rage of 450 stored shows as 45;
   - a boss's `UnitLevel` is −1;
   - an ungenerated name doesn't exist; a generated, unbuilt one raises
     its named error;
   - `ReadFile` raises "Access Denied";
   - `UnitIsUnit("target", "target")` with no target is yes, and nil
     with the correction on;
   - `UnitInRaid` for the first raid slot is 0, and 1 with the
     correction on;
   - three auras arriving together in slots 4, 5 and 6 list as 6, 5, 4
     after the auras already there; removing the first closes the gap;
   - `UnitBuff` with the filter `HARMFUL` still lists only helpful
     auras;
   - `PlayerCanTeleport` before entering the world, `GetActionInfo(145)`
     and `SummonRandomCritter()` each raise their named "the original
     does this here" error, and give the safe answer with the
     correction on.
6. The three accidents checked in the original game itself (answered
   question 1): a one-file test addon run in the original 3.3.5a client
   against the project's server, and what happened written into this
   issue's quirk table beside docs/057's answer.

## Open questions

None.

## Answered

1. ~~**Quirks with no defined result.**~~ A named error by default, the
   safe answer behind the correction switch (2026-10-20). Three of the
   original's quirks are accidents rather than behaviour:
   `PlayerCanTeleport` crashes the game when called before the player
   has entered the world; `GetActionInfo` and `GetActionText` read past
   the end of the slot list for a slot outside 1 to 144; and
   `SummonRandomCritter` hands back whatever value was left over. The
   owner: "We should use our best judgement and see if that's how the
   original game actually behaves. I'm sure they'll make the decomp
   upstream more accurate as time goes on." So:
   - **Default (the original, as near as it can be copied):** each
     raises a named error saying what the original does there, for
     example "PlayerCanTeleport before entering the world: the original
     game crashes here". An accident can't be rebuilt exactly, and the
     house rule is errors over fallbacks, so the nearest copy of "the
     game stops" is "the script stops, and says why".
   - **The correction switch:** the safe answer in the table above.
   - **Checking it against the game itself:** the decompilation can be
     wrong, so each of the three is also checked by running the
     original 3.3.5a client against the project's AzerothCore server
     with a one-file test addon that makes the call, and writing down
     what happens. That watches the program from outside and reads none
     of its code, so it stays clean-room. If the game disagrees with
     docs/057, the difference is written into this issue's quirk table
     and the client follows the game. Nothing is sent to the describer:
     the describer works uninterrupted and unsteered, and builders use
     what the finished checklist gives (the owner, 2026-10-20).

2. ~~**Unbuilt names: a loud error, or left out?**~~ The loud error
   (2026-10-20). Every name exists from the start and an unbuilt one
   raises "<name> isn't built in this client yet". The cost: an addon
   that first asks "does this function exist?" is told yes, calls it,
   and stops, where leaving the name out would let it quietly skip the
   feature. The owner: "Yes, we want the game to be complete before we
   release it. Obviously :)" So the error marks unfinished work to be
   built before release, not a lasting way for addons to tell what's
   missing, and nothing is added to let addons ask what's built.

## Related

- [docs/006](../docs/006-ui-and-addons.md),
  [docs/057](../docs/057-what-the-original-program-decides.md) notes
  1.12, 1.13 and Appendices B to D,
  [703](./703-frames-anchors-and-layers.md),
  [704](./704-events-and-the-per-frame-update.md).
