# 009 — Roadmap

Ten phases, ordered by dependency. Lower-numbered phases are foundational;
higher-numbered ones layer on. Each phase ends with a runnable demo in
`issues/completed/demos/`.

## Phase 1 — Foundation

Project skeleton, build system, the first asset reads. By the end of the
phase, we can load an MPQ, look up a file by name, and dump a parsed DBC
to stdout. No window yet.

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

Window, GL 3.3 context, math, shaders, the first textured quad. By the
end: a BLP from the live MPQ chain rendered onscreen as a textured quad
that we can move with the camera.

Capstone: [208-phase-demo](../issues/208-phase-demo.md).

## Phase 3 — World Geometry

ADT / WDT / WMO parsers + renderers, sky, lighting. By the end: stand at
a fixed spawn point in Elwynn Forest and look around, with no characters
or NPCs.

Capstone: [308-phase-demo](../issues/308-phase-demo.md).

## Phase 4 — Characters & Animation

M2 parser, skinning, animation playback, particles, ribbons. By the end:
the human male idle animation plays in the world from Phase 3.

Capstone: [408-phase-demo](../issues/408-phase-demo.md).

## Phase 5 — Networking

SRP6 auth handshake, world session crypto, packet codec, opcode table.
By the end: a real login against a local AzerothCore, receive the
character list, log no decoded errors over a minute connection.

Capstone: [508-phase-demo](../issues/508-phase-demo.md).

## Phase 6 — Login & Character Selection

The login UI, realm list, character list, character create/customize.
Drawn entirely in C (no Lua UI yet). By the end: pick a character, click
"Enter World", land in the Phase 3 world with the Phase 4 model under
the camera.

Capstone: [608-phase-demo](../issues/608-phase-demo.md).

## Phase 7 — UI Framework & Addon API

XML + Lua, FrameXML API surface, sandboxing, saved variables. By the end:
the default Blizzard UI loads. A small custom test addon also loads.

Capstone: [708-phase-demo](../issues/708-phase-demo.md).

## Phase 8 — Gameplay

Movement, spellcasting, inventory, quests, NPC interaction, looting. By
the end: complete the opening human quest in Northshire as a real player
would.

Capstone: [808-phase-demo](../issues/808-phase-demo.md).

## Phase 9 — Social

Chat (all channels), party, raid, guild, friends, who, mail, trade. By
the end: hold a chat conversation across two clients, form a party, run
through a low-level dungeon together.

Capstone: [908-phase-demo](../issues/908-phase-demo.md).

## Phase 10 — Audio & Polish & Capstone

Audio mixer + content, full-screen overlays, key binding UI, settings
panel, video options. By the end: a 30-minute supervised play session
indistinguishable from the original client at a casual glance.

Capstone: [1008-phase-demo](../issues/1008-phase-demo.md).

## How phases relate

```
1 ── 2 ── 3 ── 4         (asset → render → world → models)
     │              ╲
     │               6 ── 7 ── 8 ── 9 ── 10  (login → ui → gameplay → social → polish)
     │              ╱
     └────── 5 ────         (networking joins at login)
```

Phases 2–4 and Phase 5 can proceed in parallel once Phase 1 is done. They
converge at Phase 6.
