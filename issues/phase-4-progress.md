# Phase 4 — Characters & Animation — Progress

Models, skeletons and animation, particles and characters, for both games (docs/009).

## Issues

| ID | Title | Status | Blocked by |
|----|-------|--------|------------|
| 401 | [The M2 reader](./401-m2-reader.md) | partly built | 105 |
| 402 | [Skeletons and animation](./402-skeletons-and-animation.md) | partly built | 401 |
| 403 | [Particles](./403-particles.md) | partly built | 401, 402 |
| 404 | [Characters, equipment, creatures](./404-characters-and-equipment.md) | partly built | 401, 402 |
| 408 | [Phase 4 demo](./408-phase-demo.md) | pending | 401–404 |
| 410 | [Item pictures: icons and as worn](./completed/410-item-pictures-icons-and-as-worn.md) | completed 2026-10-23 | 404, 107 |

402's step 4 (2026-11-04) poses Warcraft III's models in C
(`src/069-mdx-anim.c`, the twin of world-edit-to-execute's
`assets/anim.lua`, held to it bit for bit by
`tests/070-mdx-anim-matches-lua.lua`), in the shape the game's own
models are bent with: each WC3 matrix group is one of the drawing's
bones. Built on 401's MDX reader; nothing draws a WC3 model yet.

410 points the character and its equipment outward. The dressing room
(`scripts/item-pictures`) draws any list of items as icons and as worn,
front and back, for pages outside the game. The first page is
wow-chat-2026's oath tabards. The pictures get better as 404's
piece and layer rules are built, with no change to 410.

Created 2026-10-07, when the code from wow-chat-2026 arrived (its issue 173v)
with these pieces already partly built.
