# Phase 8 — Gameplay — Progress

Movement, spellcasting, inventory, quests, NPC interaction, looting (docs/009).

## Issues

| ID | Title | Status | Blocked by |
|----|-------|--------|------------|
| 801 | [Movement by the server's rules](./801-movement-by-the-servers-rules.md) | partly built | 301 |
| 802 | [Collision](./802-collision.md) | partly built | 301, 304, 401 |
| 809 | [Tab targeting and the range check](./809-targeting-and-range.md) | not started | 801, 704, Phase 5 |
| 810 | [Spell description text](./810-spell-description-text.md) | not started | 106 |
| 804 | [Floating combat text over units](./804-floating-combat-text.md) | not started | 206, Phase 5 |
| 811 | [Spell visuals: stages and missiles](./811-spell-visuals.md) | not started | 402, 403, 404, Phase 5 |
| 812 | [Other units' movement](./812-other-units-movement.md) | not started | 801, Phase 5 |
| 813 | [Auto-loot](./813-auto-loot.md) | not started | 509, 704, Phase 5 |
| 808 | [Phase 8 demo](./808-phase-demo.md) | pending | 801–802, 804, 809–813 and the rest |

Created 2026-10-07, when the code from wow-chat-2026 arrived (its issue 173v)
with these pieces already partly built.

2026-10-16: the clean-room checklist
([docs/057](../docs/057-what-the-original-program-decides.md)) gave
movement and collision the original's own numbers. Several already match
(gravity, jump, fastest fall, step height); the rest are listed in 801's
and 802's current behavior as targets. Targeting and spell text were
written as 809 and 810 from the same checklist.
