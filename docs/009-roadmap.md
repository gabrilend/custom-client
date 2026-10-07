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
replacement ([012](./012-asset-replacement-and-provenance.md)). No window yet.

Issues:
- [101-build-system](../issues/101-build-system.md)
- [102-logging-and-errors](../issues/102-logging-and-errors.md)
- [103-config-loader](../issues/103-config-loader.md)
- [104-fs-abstraction](../issues/104-fs-abstraction.md)
- [105-mpq-reader](../issues/105-mpq-reader.md)
- [106-dbc-parser](../issues/106-dbc-parser.md)
- [107-blp-parser](../issues/107-blp-parser.md)
- [108-phase-demo](../issues/108-phase-demo.md)

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

Capstone: [308-phase-demo](../issues/308-phase-demo.md).

## Phase 4 — Characters & Animation

M2 parser, skinning, animation playback, particles, ribbons, the WC3 animation
behaviour layer for WC3 map mode (walk speed matching, damage point timing,
decay), team colour. The M2 reader joins the shared library. By the end: the
human male idle in Northshire; a WC3 footman-class unit walking and dying on
WC3 timing.

Capstone: [408-phase-demo](../issues/408-phase-demo.md).

## Phase 5 — Networking

SRP6 auth handshake, world session crypto, packet codec, opcode table.

Capstone: [508-phase-demo](../issues/508-phase-demo.md).

## Phase 6 — Login & Character Selection

Login UI, realm list, character list, character create. Drawn in C.

Capstone: [608-phase-demo](../issues/608-phase-demo.md).

## Phase 7 — UI Framework & Addon API

XML + Lua, FrameXML API surface, sandboxing, saved variables. By the end: the
default UI loads; so does a test addon.

Capstone: [708-phase-demo](../issues/708-phase-demo.md).

## Phase 8 — Gameplay

Movement, spellcasting, inventory, quests, NPC interaction, looting.

Capstone: [808-phase-demo](../issues/808-phase-demo.md).

## Phase 9 — Social

Chat, party, raid, guild, friends, who, mail, trade.

Capstone: [908-phase-demo](../issues/908-phase-demo.md).

## Phase 10 — Audio & Polish

Audio mixer, overlays, key binding UI, settings, video options. Sounds go
through the resolver like everything else, once it exists.

Capstone: [1008-phase-demo](../issues/1008-phase-demo.md).

## Phase 11 — WC3 Map Mode

Everything that makes a converted WC3 map playable: top-down camera with edge
pan and zoom, box select and control groups, right-click orders, the command
card and resource bar, minimap. The owner's reason for building this client:
"the purpose of creating a custom client is so that we can heavily modify it."

Capstone: [1108-phase-demo](../issues/1108-phase-demo.md).

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
