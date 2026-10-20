# 808 — Phase 8 Capstone Demo

## Current behavior

Phase 8 not started. The player stands in the world but cannot move,
cast, loot, or interact.

## Intended behavior

Play through the opening Northshire quest chain on a freshly created
human warrior:

- WASD/space movement, jump, swim.
- Target NPC, accept quest.
- Walk to mob, attack, kill, loot.
- Return to quest giver, hand in.
- Receive XP, gold, item reward.

## Suggested implementation steps

To be expanded into per-issue tickets (801–807) when Phase 8 begins.
Sketch:

- 801 — Player movement + client-side prediction.
- 802 — Collision against terrain + WMOs.
- 803 — Spellcasting + GCD + cooldowns.
- 804 — Combat log + floating combat text.
- 805 — Inventory, bags, equipment.
- 806 — Quest log + tracker + objective parser.
- 807 — NPC interaction (gossip, quest, vendor, trainer, loot windows).
- 808 — This capstone demo.

## Acceptance

- A scripted run-through completes the Northshire opener end-to-end
  with no server desyncs.

## Related

- **Blocked by:** Phase 7, [809](./809-targeting-and-range.md), [810](./810-spell-description-text.md), [804](./804-floating-combat-text.md), [811](./811-spell-visuals.md), [812](./812-other-units-movement.md), [813](./813-auto-loot.md).
