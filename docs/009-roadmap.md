# 009 — Roadmap

Eleven phases, ordered by dependency. Lower-numbered phases are foundational;
higher-numbered ones layer on. Each phase ends with a runnable demo in
`issues/completed/demos/`. Rewritten 2026-09-23 for the W client merge
([000](./000-vision-summary.md)); the earlier ten-phase version is in git
history.

Two threads run through every phase:

- **Resolver and replacement.** Once the first replacement exists, every
  asset goes through the asset resolver
  ([012](./012-asset-replacement-and-provenance.md)), and every phase demo
  prints where its assets came from (pack / blizzard / placeholder) and how
  many are past the distinctness threshold. Until then Blizzard's files are
  the placeholders, read directly (decided 2026-10-07).
- **Shared with world-edit-to-execute.** The readers built here are also a
  shared library that world-edit-to-execute calls from Lua.

## Phase 1 — Foundation

Build, logging, config, file access, MPQ chain with loose overlays, DBC with
data-described layouts, BLP read and write, the shared reader library, and
the test scenes' client half (109). The resolver waits for the first
replacement ([012](./012-asset-replacement-and-provenance.md)); built
ahead of it on 2026-11-04 (105a), with no drawing part asking it yet. No window yet.

Issues:
- [101-build-system](../issues/101-build-system.md)
- [102-logging-and-errors](../issues/102-logging-and-errors.md)
- [103-config-loader](../issues/103-config-loader.md)
- [104-fs-abstraction](../issues/104-fs-abstraction.md)
- [105-mpq-reader](../issues/105-mpq-reader.md)
- [106-dbc-parser](../issues/106-dbc-parser.md)
- [107-blp-parser](../issues/107-blp-parser.md)
- [108-phase-demo](../issues/108-phase-demo.md)
- [109-test-scenes-client-half](../issues/109-test-scenes-client-half.md)
- [110-the-ceramic-shape](../issues/110-the-ceramic-shape.md) (2026-10-07): no main loop; workers run every part as stations, render threads only draw

## Phase 2 — Rendering Core

Window, math, shaders, textures uploaded (through the resolver, once it exists), sprites, text,
cameras (including the top-down RTS camera's math). By the end: textures from
the live chain, and from a test replacement pack, drawn onscreen with their
source shown.

Capstone: [208-phase-demo](../issues/208-phase-demo.md).

## Phase 3 — World Geometry

ADT / WDT / WMO parsers + renderers, sky, lighting, streaming. Reads Blizzard
maps and world-edit-to-execute's converted WC3 maps (loose overlays) the same
way. By the end: stand in Elwynn Forest, and fly over a converted WC3 map.

Issues added 2026-10-07: [309-water](../issues/309-water.md), [310-shadows-and-live-light](../issues/310-shadows-and-live-light.md).

Capstone: [308-phase-demo](../issues/308-phase-demo.md).

## Phase 4 — Characters & Animation

M2 parser, skinning, animation playback, particles, ribbons, the WC3 animation
behaviour layer for WC3 map mode (walk speed matching, damage point timing,
decay), team colour. The M2 reader joins the shared library. By the end: the
human male idle in Northshire; a WC3 footman-class unit walking and dying on
WC3 timing.

Issues (2026-10-07): [401](../issues/401-m2-reader.md), [402](../issues/402-skeletons-and-animation.md), [403](../issues/403-particles.md), [404](../issues/404-characters-and-equipment.md).
Later (2026-10-16, after 404): [409](../issues/409-other-games-bodies.md), a Final Fantasy XIV body and its equipment as a player-side look.

Capstone: [408-phase-demo](../issues/408-phase-demo.md).

## Phase 5 — Networking

SRP6 auth handshake, world session crypto, packet codec, opcode table.

Issues (2026-10-16, from [057](./057-what-the-original-program-decides.md)): [509](../issues/509-the-cache-files.md).

Capstone: [508-phase-demo](../issues/508-phase-demo.md).

## Phase 6 — Login & Character Selection

Login UI, realm list, character list, character create. Drawn in C.

Issues (2026-10-16, from [057](./057-what-the-original-program-decides.md)): [602](../issues/602-login-results.md), [606](../issues/606-the-loading-screen.md), [609](../issues/609-losing-the-connection.md).

Capstone: [608-phase-demo](../issues/608-phase-demo.md).

## Phase 7 — UI Framework & Addon API

XML + Lua, FrameXML API surface, sandboxing, saved variables. By the end: the
default UI loads; so does a test addon.

Issues (2026-10-16, from the clean-room checklist [057](./057-what-the-original-program-decides.md)): [702](../issues/702-the-xml-reader.md), [703](../issues/703-frames-anchors-and-layers.md), [704](../issues/704-events-and-the-per-frame-update.md), [705](../issues/705-interface-functions.md), [707](../issues/707-saved-variables.md), [709](../issues/709-taint-and-combat-lockdown.md).

Capstone: [708-phase-demo](../issues/708-phase-demo.md).

## Phase 8 — Gameplay

Movement, spellcasting, inventory, quests, NPC interaction, looting.

Issues (2026-10-07): [801](../issues/801-movement-by-the-servers-rules.md), [802](../issues/802-collision.md); (2026-10-16, from [057](./057-what-the-original-program-decides.md)) [809](../issues/809-targeting-and-range.md), [810](../issues/810-spell-description-text.md), [804](../issues/804-floating-combat-text.md), [811](../issues/811-spell-visuals.md), [812](../issues/812-other-units-movement.md), [813](../issues/813-auto-loot.md).

Capstone: [808-phase-demo](../issues/808-phase-demo.md).

## Phase 9 — Social

Chat, party, raid, guild, friends, who, mail, trade.

Issues (2026-10-17, from wow-chat-2026's switch to llama.cpp): [907](../issues/907-the-model-in-the-client.md), the player's GPU lent to the server's bots.

Capstone: [908-phase-demo](../issues/908-phase-demo.md).

## Phase 10 — Audio & Polish

Audio mixer, overlays, key binding UI, settings, video options. Sounds go
through the resolver like everything else, once it exists.

Issues (2026-10-07): [1001](../issues/1001-the-options-menu.md); (2026-10-16, from [057](./057-what-the-original-program-decides.md)) [1002](../issues/1002-zone-music-and-ambience.md), [1003](../issues/1003-sounds-in-the-world.md).

Capstone: [1008-phase-demo](../issues/1008-phase-demo.md).

## Phase 11 — WC3 Map Mode

Everything that makes a converted WC3 map playable: top-down camera with edge
pan and zoom, box select and control groups, right-click orders, the command
card and resource bar, minimap. The owner's reason for building this client:
"the purpose of creating a custom client is so that we can heavily modify it."

Capstone: [1108-phase-demo](../issues/1108-phase-demo.md).

Also in Phase 11: [1109](../issues/1109-test-map-from-the-editor.md), world-edit-to-execute's Test Map button opening the edited map in this client (decided 2026-10-07 as an editor mode here; reshaped 2026-11-04: the editor keeps its own Lua renderer).

Decided 2026-09-23: AzerothCore runs a WC3 map's rules, with the map's
triggers converted into server scripts (world-edit-to-execute W02e). The client
sends orders and shows results; multiplayer comes with the server.

## How phases relate

```
1 ── 2 ── 3 ── 4 ───────────────────────────┐
     │              ╲                        ▼
     │               6 ── 7 ── 8 ── 9 ── 10  11 (WC3 map mode: needs 4 and 7)
     │              ╱
     └────── 5 ────
```

Phases 2–4 and Phase 5 can proceed in parallel once Phase 1 is done. Phase 11
needs characters (4) and the UI framework (7); it does not need 8-10.

## The crosswalk: the working code arriving (2026-10-07)

wow-chat-2026's custom client (its issues 173a to 173w; the move is its
173v) arrives as this project's first code. Much of Phases 1 to 4, and
the movement part of Phase 8, is already built there. The table maps
each of its source files to the reading-order number it takes here
(house rule: every source file starts with its index, counted across the
whole project from `.file-index-counter`, which stood at 12 for docs
000 to 012) and to the issue here that it fills. A file's `.h`, `.c` and
`.info.md` share one number. Issues marked *new* are written as the code
lands; each issue it fills gets its Current Behavior rewritten to say
what is already there.

| # | File (was) | What it is | Fills here |
|---|------------|------------|------------|
| 013 | `errors` (new) | the error codes and the sentence that goes with each | 102 |
| 014 | `settings` | the settings store (one table of declarations, the player's `Config.wtf`) | 103 |
| 015 | `mpq` | our archive reader: the chain, the shared memory of unpacked files | 105 (reading; writing still to build) |
| 016 | `dbc` | the client's tables | 106 |
| 017 | `blp` | pictures, unpacked | 107 |
| 018 | `maps` | the server's map files: the ground's heights the server uses | 801 *new*: movement by the server's rules |
| 019 | `navmesh_slope` | the steepest walkable slope, from the server's navigation tiles | 801 *new* |
| 020 | `adt` | ground files, low-detail map heights | 301 |
| 021 | `wmo` | buildings | 304 |
| 022 | `m2` | models | 401 *new*: the M2 reader |
| 023 | `anim` | skeletons, animations, poses | 402 *new*: skeletons and animation |
| 024 | `particles` | particle emitters | 403 *new*: particles |
| 025 | `character` | a character's look: pieces, painted skin, items | 404 *new*: characters and their equipment |
| 026 | `light` | the zone's light tables | 306 |
| 027 | `liquid` | the water's surface, for the mover | 309 *new*: water |
| 028 | `collide` | floors, walls, lines of sight from models' solid shapes | 802 *new*: collision |
| 029 | `mover` | one step of a character's movement | 801 *new* |
| 030 | `moves` | which animation she plays from what she does | 402 *new* |
| 031 | `camera` | the camera's styles, zoom, first person | 207 |
| 032 | `sight` | what the camera sees (the view's walls) | 307 |
| 033 | `ground_prep` | a tile's ground made ready without the window | 302 (and 303) |
| 034 | `prefetch` | the reading-ahead thread | 303 |
| 035 | `creatures` | the creature list generated from a world database (the network's stand-in, retired when the world comes from the server) | 404 *new*, until Phase 5 |
| 036 | `texture` | pictures onto the card, compressed as stored; filtering | 204 |
| 037 | `sky` | the sky dome and the light handed to shaders | 306 |
| 038 | `ground` | the ground's drawing | 302 |
| 039 | `horizon` | the far horizon from the low-detail map | 303 |
| 040 | `water` | water's drawing | 309 *new* |
| 041 | `figure` | any animated model, posed and bent on the card | 402 *new*, 404 *new* |
| 042 | `doodads` | doodads and buildings drawn, animated, particles run | 305, 401 *new*, 403 *new* |
| 043 | `main` | the window, the frame, the picture options | 201, 203 |
| 044 | `tests/test_client` | the test program | 101 |

Not moving: wow-chat-2026's first version in Lua (`src/custom-client/`),
removed there instead (173v, answer 5).

### wow-chat-2026's 173 issues and where they continue

Each 173 issue stays in wow-chat-2026 as the story of how the client
began, with a line pointing here; the issue here names it once in a
"came from" line (173v, answer 8).

| 173 | What | Continues here |
|-----|------|----------------|
| 173a | first light | 201, 302 |
| 173b, 173b1 to 173b16 | the options menu and its panels | 103 (the store), 1001 *new* (the menu) |
| 173c | interface pieces by name | Phase 7 |
| 173d | models and world objects read | 105, 107, 401 *new* |
| 173e | what the server checks | Phase 5 |
| 173f | painted ground | 301, 302 |
| 173g | doodads | 305, 401 *new* |
| 173h | shadow maps, live light | 310 *new* |
| 173i | her animation (completed) | 402 *new* |
| 173j | figures standing around (completed) | 404 *new* |
| 173k | the client in C | 101 |
| 173l | the packet tap | Phase 5 |
| 173m | the camera | 207 |
| 173n | zone light and sky | 306 |
| 173o | particles | 403 *new* |
| 173p | equipment on the body | 404 *new* |
| 173q | model collision | 802 *new* |
| 173r | water and swimming | 309 *new*, 801 *new* |
| 173s | streaming tiles (completed) | 303 |
| 173t | creatures from the world database | 404 *new*, then Phase 5 |
| 173u | rendering on the graphics card | 005, 302, 305 |
| 173w | queued inputs and test characters | 109 *new*: test scenes, the client's half |

## The second crosswalk: world-edit-to-execute's client arriving (started 2026-10-07)

world-edit-to-execute (WETE) has its own client: a small C host on raylib
(`src/render/`) that Lua drives. Lua there reads every file (WC3's
models, pictures, ground, placements), runs the game's rules, and hands
C flat arrays to draw. Here C reads and draws, and Lua is only for
scripting (docs/001). So WETE's client does not move file for file: each
part either gets a **C twin** here (checked byte for byte against WETE's
Lua once, then the Lua is deprecated and the C is the one
implementation; the owner, 2026-10-07: "Let's just deprecate the lua
version and keep the C version"),
is **replaced** by a part this client already has, or **stays in WETE**
because it is the game's rules, which run on AzerothCore (docs/004).

Found 2026-10-07: this client's own archive reader (015) already opens
WC3's `war3.mpq` and reads its models and pictures; no second archive
reader is needed for the stock game. Maps with damaged or hidden file
tables are still read by WETE through StormLib, before conversion.

| WETE part | What it is | Here |
|-----------|------------|------|
| `parsers/blp.lua`, `parsers/jpeg.lua` | WC3's pictures (BLP1: JPEG inside, or a palette) | C twin in 017 and a JPEG reader beside it (049): [107a](../issues/completed/107a-wc3-pictures-blp1.md). **Done 2026-10-07**, byte for byte with the Lua |
| `parsers/mdx.lua`, `assets/gpu.lua` | WC3 models into drawable parts | C twin filling 022's model structure: [401](../issues/401-m2-reader.md) step 2 |
| `assets/anim.lua` | WC3 sequences posed (node matrices) | the same posing as M2: [402](../issues/402-skeletons-and-animation.md) step 1 |
| particle and ribbon emitters inside `mdx.lua`; spell art (WETE 530, 538) | WC3 particles, ribbons, lightning | one particle system: [403](../issues/403-particles.md) |
| `render/models.c` | draws Lua-built meshes with team colour | replaced by 041 (figure) and 042 (doodads), which learn team colour (404) |
| `parsers/w3e.lua`, `assets/ground.lua`, `render/landscape.c`, `render/terrain.c` | WC3 ground and its tileset blending | C twin of the reading; the ground renderer (302) extended to paint WC3's tilesets and WoW's layers onto one mesh, cliffs seated on it (answer 1 below) |
| `parsers/doo.lua`, `parsers/unitsdoo.lua` | doodads and starting units placed | WETE's converter writes doodads into the tiles and units as the server's spawns |
| `render/fog.c` | fog of war drawn over the world | Phase 11, new issue when it begins; the server decides what is seen, the client only shades it |
| `render/input.c` | ground and unit picking, box select | [1108](../issues/1108-phase-demo.md)'s 1102 and 1103 |
| `render/ui2d.c`, `ui/wc3/*.lua` | the WC3 interface (command card, resource bar, portraits) | Phase 7's interface runtime, then 1105; the Lua moves as "WC3 interface (Lua)" (docs/001's layers) |
| the custom map browser (WETE 609, 1001) | finding, fetching and listing maps | stays in WETE; drawn as an HTML page onto a flat interface pane here (owner, 2026-10-07: "maybe we just render the browser to HTML and cast it onto a flat UI pane in the W client"), as the AzerothCore project's custom-model pages may be too |
| `demo/wc3map/*.lua`, `runtime/`, `jass/`, `ai/` | the game's rules: combat, economy, triggers, computer players | **not in this client**: AzerothCore runs every trigger and WC3 mechanic, in ALE Lua or by patching the server (owner, 2026-10-07); WETE's Lua is the reference the server's version is checked against |
| `render/ceramic/`, `render/threading.c`, `render/slots.c`, `render/bridge.c` | WETE's thread pool and render graph | moves: the engine becomes this client's shape, [110](../issues/110-the-ceramic-shape.md); its old pool (`threading.c`, `slots.c`, `bridge.c`) does not |
| `render/geometry.c`, `geometry/*.lua` | flat-shaded solids painted from designs | moves as placeholder art, and the first test of the custom model system (answer 2 below) |
| `net/` | WETE's own message server | superseded for both games by AzerothCore; WETE's W08 (its own server) is open |
| `editor/` | the world editor | stays in WETE and keeps WETE's Lua renderer for editing (owner, 2026-11-04: "the lua renderer is only for the editor, not the client"); its Test Map button opens the edited map here: [1109](../issues/1109-test-map-from-the-editor.md). Decided 2026-10-07 as a mode of this client, with WETE's renderer retiring; reshaped 2026-11-04. So the `render/` rows above say what this client draws for play; WETE's copies stay for the editor |

### Open questions for the second crosswalk

1. ~~WC3 ground: converted or read directly?~~ Answered 2026-10-07: the
   ground renderer is extended to draw both kinds, "We should be able to
   paint both onto the same terrain mesh. Cliffs placed onto a terrain
   mesh should jump to the highest point that their lowest point
   intersects with the terrain... or something similar" (the owner). So
   one ground mesh takes WoW's layered painting and WC3's tileset
   painting, and WC3 cliffs are models seated on that mesh by rule.
2. ~~Flat-shaded designs~~ Answered 2026-10-07: they come here as
   placeholder art, "We can use them for a test of the custom model
   system" (the owner).
3. ~~The thread pool~~ Answered 2026-10-07: the ceramic engine becomes
   this client's shape, "I'd prefer not to have any main thread at all,
   or if we do, that it's only the rendering thread. Ideally we'd be able
   to have multiple rendering threads for different openGL contexts"
   (the owner). [110](../issues/110-the-ceramic-shape.md).
