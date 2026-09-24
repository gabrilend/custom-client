# Phase 1 — Foundation — Progress

The goal of Phase 1 is to build the foundation: a working build, a logging
and error model, configuration loading, filesystem abstraction, and the
ability to read MPQ archives and parse the DBC and BLP formats inside them.
The phase ends with a runnable demo that touches every Phase 1 piece.

## Issues

| ID  | Title                                           | Status   | Blocked by |
|-----|-------------------------------------------------|----------|------------|
| 101 | [Build system](./101-build-system.md)           | pending  | —          |
| 102 | [Logging and errors](./102-logging-and-errors.md) | pending  | 101        |
| 103 | [Config loader](./103-config-loader.md)         | pending  | 101, 102   |
| 104 | [Filesystem abstraction](./104-fs-abstraction.md) | pending  | 103        |
| 105 | [MPQ chain (read and write)](./105-mpq-reader.md) | pending  | 104        |
| 105a | [Loose overlays and asset resolver](./105a-loose-overlays-and-asset-resolver.md) | pending | 104, 105 |
| 106 | [DBC parser with data-described layouts](./106-dbc-parser.md) | pending  | 105        |
| 107 | [BLP read and write](./107-blp-parser.md)       | pending  | 105        |
| 108 | [Phase 1 demo](./108-phase-demo.md)             | pending  | 102–107    |

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
