# 701 — The Lua Environment

*Written 2026-11-04 from [docs/006](../docs/006-ui-and-addons.md)
("Loading order" step 1, "Sandboxing", "Editing / debugging"),
[docs/002](../docs/002-technology-stack.md) (LuaJIT 2.1, its `ffi` never
an addon's) and [docs/057](../docs/057-what-the-original-program-decides.md)
section 1, at the number Phase 7's capstone sketched for "embed LuaJIT,
define the addon environment table". Background work the owner asked
for; not built, since the machine it was written on has no LuaJIT
headers.*

## Blocked by

- Nothing in this project. It needs LuaJIT 2.1 on the build machine
  (below).

## Blocks

- [702](./702-the-xml-reader.md) (running the scripts its files hold),
  [703](./703-frames-anchors-and-layers.md),
  [706](./706-the-addon-loader.md), [707](./707-saved-variables.md),
  [709](./709-taint-and-combat-lockdown.md), [708](./708-phase-demo.md).

## Current behavior

Nothing runs Lua. The interface's XML is read (702, `src/066-framexml.c`)
and its scripts are kept as text: each `<Script>` as a step in load
order, each handler (`<OnLoad>`, `<OnEvent function="...">`) on the frame
it belongs to, each with the file, the line it starts on and the taint
of the code that defined it. Nothing runs them yet.

## Intended behavior

One Lua state runs the whole interface: the default interface's Lua and
every addon's, unchanged from what they were written for (docs/006:
"WoW addons must work as-is").

- **LuaJIT 2.1** (docs/002), linked as [docs/008](../docs/008-build-system.md)
  says: the system's, as zlib is, until libraries come from `libs/` (the
  plan docs/008 records for raylib; the owner prefers vendored local
  libraries, so LuaJIT goes the same way when that lands). Built in its
  64-bit garbage-collected mode (LuaJIT 2.1's default on x86-64), so the
  interface's memory isn't held to 2 GB.
- **One global table, shared.** The default interface and every addon
  read and write the same globals: addons call each other's functions
  and the default interface's, and XML names global functions in
  `function="..."`. docs/006's "each addon gets its own environment
  table" is read here as an environment per addon that passes every
  read and write through to the one shared table, so the code sees what
  it would in the original, while the environment knows whose code is
  running. That is where 709's side table of labels (who last wrote each
  global) is kept up, as 709's answer 2 asks; 709 builds the labels, this
  issue builds the place for them.
- **What the scripts are given** (docs/006, "Sandboxing"): Lua 5.1's
  standard library without `io`, most of `os`, `package.loadlib` and the
  `debug.set*` functions; no `require`, `dofile` or `loadfile` (the
  original reads files only through the interface's own loading); none
  of LuaJIT's own: `ffi` and `jit` are never reachable from a script
  (docs/002, docs/006: "LuaJIT's `ffi` is the runtime's tool, not a
  script's"). The exact set of globals the original gives, and the
  extras its Lua carries (`bit`, `strsplit`, `wipe`, the short names
  such as `tinsert` and `format`), are docs/057 Appendix C's; the
  library extras are built with [705](./705-interface-functions.md),
  against that list.
- **C functions on the state** (docs/006, "Loading order" step 1): one
  table of names and C functions, registered before the default
  interface loads. Each part adds its own (703's widget methods, 704's
  events, 705's functions); this issue gives the registering and the
  rules every C function keeps (arguments read by docs/057 note 1.15's
  on/off rules, the original's usage text as the error a wrong call
  raises).
- **The XML's scripts, run where they stand** (702): the Lua environment
  walks 702's steps in order, running each `<Script file>` and inline
  `<Script>` before the frames written after it are made, as the
  original does (a frame's `OnLoad` can call a function its file
  defined just above it). Each handler is compiled once, as a function
  taking the handler's own arguments (`self`, then `event, ...` for
  `OnEvent`, `elapsed` for `OnUpdate`, `button, down` for `OnClick`; the
  full list is docs/057 Appendix B's), with its chunk named after the XML
  file so its errors read `Interface\AddOns\X\X.xml:12: ...`, its line
  numbers counted from the line 702 gives. A handler written in a
  template runs with the taint of the code that defined it (702's mark,
  709's rules).
- **Errors are never silent** (docs/006, "Editing / debugging"): an
  uncaught error goes to the error handler (`seterrorhandler`,
  `geterrorhandler`, docs/057 Appendix C), whose default writes it to the
  terminal and the interface log until the interface's own error frame
  exists, and shows that frame when `scriptErrors` is 1. Loading carries
  on after a script's error, as the original does.
- **Window-free.** The state and its loading need no window, so the
  test program runs them: it joins the CORE list in `scripts/build`,
  not the reader library (world-edit-to-execute has its own LuaJIT and
  no use for the interface's).

## Suggested implementation steps

1. The build: LuaJIT's header and library found on the build machine,
   with a clear error naming them when missing, as raylib's is; then
   from `libs/` when docs/008's plan for it lands.
2. The state as a handle (docs/001, "Handles"), its standard library cut
   to the list above, `ffi` and `jit` out of reach (also through
   `package.loaded` and `getfenv` / `getmetatable` on library functions).
3. The per-addon environments over the one global table, with the hook
   709's labels will use.
4. Registering C functions, and the argument rules they share.
5. Running 702's steps and handlers, with chunk names and lines.
6. The error handler and its default.
7. Tests:
   - a script calling `io.open`, `require("ffi")`, `jit.off()` or
     `package.loadlib` gets the same "attempt to ..." error it would in
     the original, never the thing;
   - two addons share a global: one defines `Foo`, the other calls it;
   - an XML file whose `<Script>` defines a function, then a frame whose
     `OnLoad` calls it: the call succeeds;
   - an error in a handler written on line 12 of `X.xml` reads
     `X.xml:12:` and the next file still loads;
   - a handler from a trusted template, on an addon's frame, runs with
     the trusted mark (checked once 709 exists).

## Acceptance

- The default interface's Lua files compile and run in the state with
  no error that the original doesn't also give.
- No script can reach `ffi`, `jit`, `io` or the file system by any path
  the tests try.

## Decisions

Made 2026-11-04 without the owner (background work); each is easy to
turn round.

- **Shared globals over separate ones.** docs/006 asks for an
  environment per addon; taken literally (each addon its own globals),
  addons could no longer call each other or be called from XML, so
  "work as-is" wins and the environment is a pass-through that knows
  whose code runs (above).
- **CORE, not the reader library**, as above.

## Open questions

1. **`goto`.** LuaJIT 2.1 reads `goto` as a keyword; the original's Lua
   5.1 reads it as an ordinary name, so an addon with a variable called
   `goto` fails to compile here. Expected to be rare; if one turns up,
   the choice is a LuaJIT build option (none exists for this today) or
   rewriting the name on load, which docs/006's "as-is" argues against.
2. **What a file's `...` holds.** The original hands each addon file
   some arguments (its addon's name, and possibly a table its files
   share); docs/057 doesn't yet say which, from which version. A row for
   section 1.

## Related

- [docs/006](../docs/006-ui-and-addons.md), [docs/002](../docs/002-technology-stack.md),
  [docs/008](../docs/008-build-system.md),
  [docs/057](../docs/057-what-the-original-program-decides.md) section 1,
  [702](./702-the-xml-reader.md), [706](./706-the-addon-loader.md),
  [709](./709-taint-and-combat-lockdown.md).
