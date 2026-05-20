# 708 — Phase 7 Capstone Demo

## Current behavior

Phase 7 not started. No Lua VM is wired in, no XML parser, no addon
loader, no FrameXML.

## Intended behavior

The Phase 6 in-world view is rebuilt as a FrameXML/Lua UI:

- The C-side temporary UI from Phase 6 is replaced by the actual
  `Interface/FrameXML/*.xml` + `*.lua` from the live MPQ chain.
- The default Blizzard action bars, unit frames, chat frame, minimap,
  buff bars all appear, looking like vanilla 3.3.5a.
- A trivial custom test addon ships under `addons/TestAddon/` and prints
  to chat on `PLAYER_LOGIN`.
- Saved variables persist between sessions for the test addon.

## Suggested implementation steps

To be expanded into per-issue tickets (701–707) when Phase 7 begins.
Sketch:

- 701 — Embed LuaJIT, define the addon environment table.
- 702 — XML pull-parser (FrameXML subset).
- 703 — Widget tree + anchor/size primitives in C, scriptable from Lua.
- 704 — Event system (RegisterEvent / OnEvent dispatch).
- 705 — FrameXML API surface (CreateFrame, Get/SetText, UnitName, …).
- 706 — Addon loader (.toc parser, dependency sort, load).
- 707 — Saved variables (load on startup, write on logout).
- 708 — This capstone demo.

## Acceptance

- The default UI loads without errors; `/console scriptErrors 1` shows
  no uncaught Lua errors during 10 minutes of standing-in-world.
- The test addon loads, fires `PLAYER_LOGIN`, persists a saved variable
  across a restart.

## Related

- Blocked by: Phase 6.
