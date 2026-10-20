# 006 — UI and Addons

The UI is the project's largest compatibility surface. Blizzard's 3.3.5a UI
is itself written as a Lua + XML addon ("FrameXML"). The same parser and
runtime that loads a third-party addon also loads the default UI. If that
works, we get the addon ecosystem essentially for free.

**WoW addons must work as-is** (the owner, 2026-10-09: "It's important
that WoW addons work as-is"): an addon written for the original 3.3.5a
client loads and runs here without being changed. The map editor's own
panels are built on this same runtime, so it is the one way interface is
made in this client (issue 1109).

## What an addon is

A directory with a `.toc` manifest and a tree of Lua and XML files. The
manifest lists files in load order, declared saved-variable names, declared
dependencies, and metadata (title, author, interface version).

## Loading order

1. **Boot-time C-side init**: register the FrameXML API surface as C
   functions on the Lua state.
2. **Default UI** (`Interface/FrameXML/FrameXML.toc`) loads first.
3. **Enabled addons** load in dependency order (topological sort over
   `Dependencies` and `OptionalDeps`). Cycles → typed error, addon disabled.
4. The `ADDON_LOADED` event fires per addon as its `.toc` finishes.
5. Once the player picks a character, `PLAYER_LOGIN` and the rest of the
   event flow follows real-server semantics.

## Sandboxing

Each addon runs in the same Lua state but each gets its own environment
table (set via `setfenv` on the chunk). The shared environment exposes:

- Lua 5.1 standard library, minus `io`, minus most of `os`, minus
  `package.loadlib`, minus `debug.set*`.
- The full FrameXML API (CreateFrame, RegisterEvent, GetTime, …).
- Saved variables backed by a per-addon SavedVariables file written to
  `WTF/Account/<account>/SavedVariables/<addon>.lua` on logout.

The C-side FFI is **never** exposed to addons. LuaJIT's `ffi` is the
runtime's tool, not a script's.

## The FrameXML API surface

Implemented in C, registered on the Lua state. Major groups:

| Group              | Examples                                                      |
|--------------------|---------------------------------------------------------------|
| Frame construction | `CreateFrame`, `<Frame>` XML tag, anchor + size primitives    |
| Events             | `RegisterEvent`, `UnregisterEvent`, `OnEvent`                 |
| World queries      | `UnitName`, `UnitHealth`, `UnitClass`, `GetTime`              |
| Combat             | `CombatLogGetCurrentEventInfo`, COMBAT_LOG_EVENT_UNFILTERED   |
| Spellbook          | `GetSpellInfo`, `CastSpell`, `IsUsableSpell`                  |
| Inventory          | `GetContainerItemInfo`, `UseContainerItem`                    |
| Chat               | `SendChatMessage`, `ChatFrame_AddMessage`                     |
| Macros             | `RunMacro`, `RunMacroText` (secure)                           |
| Secure templates   | The 3.3.5a secure code restriction model — out-of-combat-only |

The secure-template model is the most subtle piece. Combat-relevant calls
(targeting, ability casting, movement) can only originate from code that
was loaded into the "secure" environment by the default UI or by an
explicit secure-template hookup. This is what stops a malicious addon from
fully botting a character.

## XML parsing

FrameXML XML is a small subset: declared elements, declared attributes,
no namespaces, no DTDs, no CDATA outside `<Scripts>` blocks. A hand-written
pull-parser is faster to build, faster to run, and zero-dep compared to
pulling in libxml2.

## Editing / debugging

`/console scriptErrors 1` opens a Lua error frame on uncaught errors. We
implement it the same way — uncaught Lua errors surface to a built-in
debug frame, not silently swallowed.

## Phase plan

The UI framework lands in Phase 7 once the world is renderable and
networking is up. Before then, the only on-screen text is rendered by C.

The original's behaviour the runtime must match (what the program, not
the interface's files, decides) is collected in the clean-room
checklist, [057](./057-what-the-original-program-decides.md): section 1
(anchors and rectangles, scale, strata and levels, drawing against
input order, the per-frame update, who hears an event and in what
order) with its quirks and their correction switches; section 7 (the
spell-description tokens tooltips are built from); section 8 (tab
targeting, the range check behind `IsActionInRange`, nameplate
distance). Phase 7's issues build from those notes when they are
written.

See [009-roadmap.md](./009-roadmap.md).
