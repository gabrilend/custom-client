# Phase 1 — Foundation — Progress

The goal of Phase 1 is to build the foundation: a working build, a logging
and error model, configuration loading, filesystem abstraction, and the
ability to read MPQ archives and parse the DBC and BLP formats inside them.
The phase ends with a runnable demo that touches every Phase 1 piece.

## Issues

| ID  | Title                                           | Status   | Blocked by |
|-----|-------------------------------------------------|----------|------------|
| 101 | [Build system](./101-build-system.md)           | partly built | —          |
| 102 | [Logging and errors](./102-logging-and-errors.md) | partly built | 101        |
| 103 | [Config loader](./103-config-loader.md)         | partly built | 101, 102   |
| 104 | [Filesystem abstraction](./104-fs-abstraction.md) | pending  | 103        |
| 105 | [MPQ chain (read and write)](./105-mpq-reader.md) | partly built | 104        |
| 105a | [Loose overlays and asset resolver](./105a-loose-overlays-and-asset-resolver.md) | pending | 104, 105 |
| 106 | [DBC parser with data-described layouts](./106-dbc-parser.md) | partly built | 105        |
| 107 | [BLP read and write](./107-blp-parser.md)       | partly built | 105        |
| 107a | [WC3's pictures (BLP1)](./completed/107a-wc3-pictures-blp1.md) | completed | 105 |
| 108 | [Phase 1 demo](./108-phase-demo.md)             | pending  | 102–107    |
| 109 | [Test scenes: the client's half](./109-test-scenes-client-half.md) | pending | 101; wow-chat-2026 176a |
| 110 | [The ceramic shape: no main loop, only render threads](./110-the-ceramic-shape.md) | pending (110a-110f) | 101, 201 |

## Suggested order

101 → 102 → 103 → 104 → 105 → (105a ∥ 106 ∥ 107) → 108

Rewritten 2026-09-23 for the W client merge ([docs/000](../docs/000-vision-summary.md)):
the reading layer is also a shared library for world-edit-to-execute, and
every asset goes through the resolver.

## Phase-end deliverable

A headless capstone binary, runnable via `./run-phase-demo.sh` → `1`, that
opens the live MPQ chain and prints stats about it: archives loaded,
override count, Map.dbc + ChrRaces.dbc contents, a decoded BLP dumped to
`tmp/phase-1-demo/` as a TGA.

## Updates

_(append entries here as issues complete, newest at the bottom)_

2026-10-07: the code from wow-chat-2026's custom client arrived (its issue
173v). "Partly built" marks an issue whose Current behavior now describes
built code; each says what is left. Counts: `lua /home/ritz/programming/ai-stuff/scripts/progress-dashboard.lua /mnt/mtwo/games/azeroth-core/custom-client -m`.

2026-10-07: 107a completed: WC3's pictures (BLP1, with a JPEG reader of
the client's own) are read, the same bytes as world-edit-to-execute's
Lua reader on every picture in WC3's archives. The first step of
world-edit-to-execute's client moving here (docs/009, the second
crosswalk). BLP writing stays open in 107.

2026-10-07: 110 written with 110a-110f: the client becomes a ceramic
program, with workers and render threads only (the owner's choice; the
design and measurements from world-edit-to-execute 515).
