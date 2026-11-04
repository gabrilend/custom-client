# Phase 7 — UI Framework & Addon API — Progress

XML and Lua, the FrameXML API surface, sandboxing, saved variables; by its
end the default UI loads, and so does a test addon (docs/009, docs/006).
Not started: no Lua VM is wired in, no XML parser, no addon loader.

## Issues

| ID | Title | Status | Blocked by |
|----|-------|--------|------------|
| 702 | [The XML reader](./702-the-xml-reader.md) | not started | the Lua environment (not yet written) |
| 703 | [Frames: anchors, scale, and layers](./703-frames-anchors-and-layers.md) | not started | 702, the Lua environment |
| 704 | [Events and the per-frame update](./704-events-and-the-per-frame-update.md) | not started | 703 |
| 705 | [The interface functions](./705-interface-functions.md) | not started | 703, 704 |
| 707 | [Saved variables](./707-saved-variables.md) | not started | 704 |
| 709 | [Taint, protected functions and combat lockdown](./709-taint-and-combat-lockdown.md) | not started | 704 |
| 710 | [A web browser addon](./710-a-web-browser-addon.md) | not started (decided 2026-11-04) | 701-706, 110f |
| 708 | [Phase 7 demo](./708-phase-demo.md) | pending | Phase 6, 702–704, 707, 709 |

These were written 2026-10-16 from the clean-room checklist's section 1
([docs/057](../docs/057-what-the-original-program-decides.md)), at the
numbers the capstone's sketch had given them where it had one. Still to
be written from docs/009 and docs/006: the Lua environment (701) and the
addon loader (706).

Created 2026-10-09, when the documentation site's progress chart showed this
phase's capstone with no progress table.
