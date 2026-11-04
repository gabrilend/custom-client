# Phase 7 — UI Framework & Addon API — Progress

XML and Lua, the FrameXML API surface, sandboxing, saved variables; by its
end the default UI loads, and so does a test addon (docs/009, docs/006).
Started 2026-11-04: the XML reader is built (702, not yet merged); no Lua
VM is wired in and no addon loader exists.

## Issues

| ID | Title | Status | Blocked by |
|----|-------|--------|------------|
| 701 | [The Lua environment](./701-the-lua-environment.md) | not started | LuaJIT on the build machine |
| 702 | [The XML reader](./702-the-xml-reader.md) | partly built | 701 (running its scripts), 709 (acting on their taint) |
| 703 | [Frames: anchors, scale, and layers](./703-frames-anchors-and-layers.md) | not started | 702, 701 |
| 704 | [Events and the per-frame update](./704-events-and-the-per-frame-update.md) | not started | 703 |
| 705 | [The interface functions](./705-interface-functions.md) | not started | 703, 704 |
| 706 | [The addon loader](./706-the-addon-loader.md) | not started | 701, 702, 704 |
| 707 | [Saved variables](./707-saved-variables.md) | not started | 704, 706 |
| 709 | [Taint, protected functions and combat lockdown](./709-taint-and-combat-lockdown.md) | not started | 704 |
| 710 | [A web browser addon](./710-a-web-browser-addon.md) | not started (decided 2026-11-04) | 701-706, 110f |
| 708 | [Phase 7 demo](./708-phase-demo.md) | pending | Phase 6, 701–707, 709 |

702 to 705, 707 and 709 were written 2026-10-16 from the clean-room
checklist's section 1 ([docs/057](../docs/057-what-the-original-program-decides.md)),
at the numbers the capstone's sketch had given them where it had one;
701 (the Lua environment) and 706 (the addon loader) were written
2026-11-04 from docs/006, docs/002 and docs/057 section 1.

Created 2026-10-09, when the documentation site's progress chart showed this
phase's capstone with no progress table.

## Completed

_(append entries here as issues complete, newest at the bottom)_

2026-11-04: 702's window-free part built on branch
`claude/framexml-reader`: the pull-parser (`src/065-xml.c`) and the
element tree with templates, inheriting, parents, `$parent`, anchors,
scripts kept with their taint, and the interface log
(`src/066-framexml.c`), both in the reader library; 52 checks in the
test program on files it writes, alone and under the address and
undefined-behaviour checkers. Running the scripts waits for 701, acting
on their taint for 709. 701 and 706 written.
