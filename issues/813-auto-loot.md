# 813 — Auto-loot

*Written 2026-10-16 from the clean-room checklist's note 8.9
([docs/057](../docs/057-what-the-original-program-decides.md)). The loot
window itself belongs to the interaction work sketched in Phase 8's
capstone; this issue is what the program decides before and instead of
showing it.*

## Blocked by

- [509](./509-the-cache-files.md) (every item cached before deciding),
  [704](./704-events-and-the-per-frame-update.md) (the loot events),
  Phase 5 (the loot messages).

## Blocks

- [808](./808-phase-demo.md) ("kill, loot").

## Current behavior

Nothing exists. There's no looting.

## Intended behavior

Looting takes items automatically, or opens the window, exactly when
the original does (note 8.9):

- **On or off for this loot:** `autoLootDefault` (default 0), flipped
  while the `AUTOLOOTTOGGLE` modified click is held, fixed when looting
  starts; the loot cursor shows its auto-loot form whenever the answer
  would be on.
- **Loot opened by a spell**, fixed when the spell lands: pick-pocketing
  and skinning auto-loot exactly when the setting is on; disenchanting,
  prospecting and milling only when it's on and the toggle isn't held.
  The toggle can never turn it on here (a **quirk**, below).
- **When the contents arrive, with auto-loot on:** wait until every item
  is cached; take the money; go through the slots in order, considering
  only slot types 0 and 4; a bind-on-pickup item of uncommon quality or
  better without item flag 2048 is skipped unless its slot type is 4,
  and the first such item fires `LOOT_BIND_CONFIRM` with its slot; every
  other item is taken with its sound; auto-loot then switches off for
  this loot; if nothing is left the window closes without opening,
  otherwise `LOOT_OPENED` fires with 1.
- **Without auto-loot:** `LOOT_OPENED` with 0 once every item is cached.
- **Slot numbers:** money, when present, is slot 1, and every item's
  number is one higher.

### Quirk, with a correction switch (off by default: the original)

| Quirk (8.9) | The original | The correction |
|---|---|---|
| the toggle on spell loot | for loot opened by a spell, the toggle can turn auto-loot off but never on | use the flipped answer here too |

## Suggested implementation steps

1. The on-or-off answer, fixed at the start, and the cursor.
2. The spell cases.
3. Taking items in order with the slot-type and bind-on-pickup rules.
4. Tests (with scripted loot messages):
   - with the setting on, a corpse with money and two ordinary items is
     emptied and no window opens;
   - a bind-on-pickup rare item stays, fires `LOOT_BIND_CONFIRM` with
     slot 2 when there's money, and the window opens;
   - holding the toggle with the setting off auto-loots a corpse, but
     not a skinning (and does with the correction on);
   - nothing is decided until the last item's cache answer arrives.

## Open questions

None.

## Related

- [docs/057](../docs/057-what-the-original-program-decides.md) note 8.9.
