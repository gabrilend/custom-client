# 1108 — Phase 11 Capstone Demo (WC3 Map Mode)

## Current behavior

Phase 11 not started. Converted WC3 maps can be viewed (Phases 3-4) but not
played: there is no RTS camera controller, selection, orders or command card.

## Intended behavior

A converted WC3 map (from world-edit-to-execute) played in the W client with
WC3's controls:

- Top-down camera: steep fixed pitch, zoom, edge and arrow-key pan, minimap
  click to jump.
- Box select, shift-add, control groups 1-0, double-click to select all of a type.
- Right-click to move or attack; attack-move; stop; hold position.
- Command card (3 × 4 grid) with hotkeys, resource bar (gold, lumber, food),
  selection panel with portraits.
- HUD statistics: units selected, orders issued per minute, order-to-response
  latency (ms), frame time, asset sources and replacement progress.

## Suggested implementation steps

To be expanded into per-issue tickets (1101–1107) when Phase 11 begins.
Sketch:

- 1101 — RTS camera controller (on the Phase 2 camera, [207](./207-camera.md)).
- 1102 — Ground picking (screen ray → terrain height field) and unit picking.
- 1103 — Selection model: box, shift, groups, double-click.
- 1104 — Orders and their path to whoever runs the rules (see open question).
- 1105 — Command card, resource bar, selection panel (on the Phase 7 UI framework).
- 1106 — Minimap for WC3 maps.
- 1107 — Loading a converted map from its overlay folder; start locations.
- 1108 — This capstone demo.

## Who runs the rules (decided 2026-09-23)

**AzerothCore**, with the map's triggers converted to server scripts
(world-edit-to-execute W02e). The client stays a viewer and an order-sender:
1104 sends orders to the server and shows what the server answers.
Multiplayer comes with the server. The cost: every WC3 rule must be
re-expressed as server logic, and WC3 details (attack damage point, order
queueing, collision sizes) must survive the translation.

The option not taken was running world-edit-to-execute's own simulation inside
this client. It would run the rules exactly as written, but it needs its own
lockstep networking for multiplayer and puts a second Lua world beside the
sandboxed addon one.

Licences: the client talks to AzerothCore only over the network protocol, so
it may have its own licence, provided it copies no AzerothCore code. See
world-edit-to-execute `docs/licensing-and-boundaries.md`.

## Acceptance

- `./run-phase-demo.sh` → choose 11 → a converted WC3 map, playable with
  WC3 controls for five minutes without errors.

## Related

- Blocked by: Phase 4, Phase 7.
- world-edit-to-execute W02 (conversion), W03 (behaviour layer).
