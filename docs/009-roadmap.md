# 009 — Roadmap

Eleven phases, ordered by dependency. Lower-numbered phases are foundational;
higher-numbered ones layer on. Each phase ends with a runnable demo in
`issues/completed/demos/`. Rewritten 2026-09-23 for the W client merge
([000](./000-vision-summary.md)); the earlier ten-phase version is in git
history.

Two threads run through every phase:

- **Resolver and replacement.** From Phase 1 on, every asset goes through the
  asset resolver ([012](./012-asset-replacement-and-provenance.md)), and every
  phase demo prints where its assets came from (pack / blizzard /
  placeholder) and how many are past the distinctness threshold.
- **Shared with world-edit-to-execute.** The readers built here are also a
  shared library that world-edit-to-execute calls from Lua.

## Phase 1 — Foundation

Build, logging, config, file access, MPQ chain with loose overlays, DBC with
data-described layouts, BLP read and write, the asset resolver's first cut,
and the shared reader library. No window yet.

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

Window, math, shaders, textures uploaded through the resolver, sprites, text,
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
through the resolver like everything else.

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
