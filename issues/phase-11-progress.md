# Phase 11 — WC3 Map Mode — Progress

Everything that makes a converted Warcraft III map playable: a top-down
camera with edge pan and zoom, box select and control groups, right-click
orders, the command card and resource bar, the minimap (docs/009). The
owner's reason for building this client: "the purpose of creating a custom
client is so that we can heavily modify it." Converted maps can already be
viewed (Phases 3 and 4) but not played; the map editor is to become a mode
of this client.

## Issues

| ID | Title | Status | Blocked by |
|----|-------|--------|------------|
| 1108 | [Phase 11 demo](./1108-phase-demo.md) | pending | Phase 4, Phase 7 |
| 1109 | [Test Map: the editor opens its map in the W client](./1109-test-map-from-the-editor.md) | in progress: step 1 built 2026-11-04 (the launch option and the goodbye receipt; on 105a's resolver, not on main yet); step 1b (the converted map opened) 2026-11-04; step 1c (its doodads and buildings, with their own models and pictures, through the resolver) 2026-11-05; step 1d (the player's Warcraft III install as a resolver place, and the converted ground painted with WC3's own tilesets from `war3map.w3e`) 2026-11-05; step 1e (its cliffs as WC3's own cliff models, on their cliff's ground) 2026-11-05. Decided 2026-10-07, reshaped 2026-11-04: the editor keeps its own renderer | 401, 302, 105a, world-edit-to-execute's W02 |
| 1110 | [A hosted map's models come with it](./1110-a-hosted-maps-models-come-with-it.md) | pending (decided 2026-11-04; from world-edit-to-execute's pull request #227) | 105a, Phase 7, 707, 1101-1107 |

The capstone names the issues to be written when the phase begins
(1101 the RTS camera, 1102 picking, 1103 selection, 1104 orders, 1105 the
command card, 1106 the minimap, 1107 loading a converted map); 1109 shares
1101 to 1103.

2026-11-04: 1109's step 1. `--test-map FOLDER --receipt FILE` (with
`--camera`, `--realm`, `--account`, the password from
`W_CLIENT_PASSWORD`) opens the client with the map's folder first of the
resolver's overlays and writes a goodbye receipt when it ends, which the
editor reads back; the contract is in 1109. 52 checks in the test
program; the main program's part is compile-checked only (no raylib
where it was written).

2026-11-05: 1109's step 1c. A test-map run draws the map's placed
doodads and buildings through the resolver: its own WC3 models
(`.mdx`, from its build folder, sized by `map.lua`'s rule, standing
still), the game's `.m2` from the first place that has them, the
magenta wire-frame box for a model no place has, and the ground's and
models' pictures (the checker for a missing one). Checked on two real
conversions by W02c (War in the Plaguelands: 11034 doodads, 118 of its
own models read; Master Crafters: 2940 doodads); drawing compile-checked
only. Decisions 20 to 31 in 1109.

2026-11-05: 1109's step 1d. The player's own Warcraft III install
(setting `assetWc3`: a folder under `assets` with war3.mpq, War3x.mpq,
War3Patch.mpq, or a link to the install) is a resolver place, read
after the game's archives, so stock WC3 pictures and models a converted
map names come from there before the checker or box. PR #3's `.w3e`
reader merged in, a SYLK reader added (`077-slk.c`, for
`TerrainArt\Terrain.slk`), and a converted map whose build carries
`war3map.w3e` has its ground painted with WC3's own tilesets
(`078-wc3-ground.c`, through 033's ground pictures and blend: no new
shader). On War in the Plaguelands and Master Crafters (their `.w3e`
copied in by hand: the converter doesn't copy it yet) every WC3 cell
lands on one chunk cell, and chunks of four tilesets or fewer match
WC3's own laying pixel for pixel (90.4% and 99.8% of all blend points).
No real WC3 install checked; drawing compile-checked only. Decisions 32
to 41 in 1109.

2026-11-05: 1109's step 1e. With the player's WC3 install, a converted
map's cliffs stand as WC3's own cliff models (`079-wc3-cliffs.c`: each
cell on different cliff levels and off a ramp gets the model its
corners' levels name, from `TerrainArt\CliffTypes.slk`'s folder, read
through the resolver), added to each tile's doodads so 042 draws them
with their cliff tileset's picture, and their cells laid with that
tileset's ground. Checked on War in the Plaguelands (498 cliffs) and
Master Crafters (2002 placed; 980 ramp cells left for the next step)
with made sheets and models: each placed once, named and placed by the
rule, its foot on its own level. Real cliff models unchecked (no
install); drawing compile-checked only. Decisions 42 to 50 in 1109.

Created 2026-10-09, when the documentation site's progress chart showed this
phase's issues with no progress table.
