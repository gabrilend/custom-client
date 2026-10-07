# The W Client - Claude Instructions

One open client, written in C on raylib, for the World of Warcraft 3.3.5a
world (talking to an AzerothCore server) and for Warcraft III maps
converted by world-edit-to-execute. The working code arrived from
wow-chat-2026's custom client on 2026-10-07 (its issue 173v).

## Session start

Read, in order: `docs/000-vision-summary.md`, `docs/001-architecture-overview.md`,
`docs/009-roadmap.md` (with its crosswalk from wow-chat-2026's 173 issues),
then `docs/045-datapath.md` for the code as it is. Each source file in
`src/` has a `.info.md` beside it: read that before the source.

## Compiling

**Claude may build this client** with `scripts/build --test` (the owner,
2026-10-06, of the same code in wow-chat-2026: "this project in it's
current stage will compile very quickly"). The script prints how long
each build took and appends it to `tmp/shared-memory/build-times.log`.
**If a build ever takes more than about five minutes, stop building it
yourself**, tell the owner, and hand builds to them. The script prints a
warning when that line is crossed.

The checked builds are `scripts/build --asan` (memory used after freeing
or past its end) and `scripts/build --tsan` (two threads touching memory
with no lock between). Pictures: `scripts/run --shot` and its options
(in its header).

## Where things are

- `src/`: the engine, numbered in reading order (`.file-index-counter`
  holds the highest number in use).
- `tests/044-test-client.c`: the one test program.
- `assets/` (git-ignored): `server-data/` (a link to an AzerothCore
  server's extracted maps, mmaps and vmaps), `world/` (generated creature
  lists, until the world comes from the server), `settings/Config.wtf`.
- The original client's archives are read in place from its `Data`
  folder (`CLIENT`, default in `scripts/run`).

## Rules this code keeps

- Every part keeps its state behind a handle; nothing in a file but
  constants (docs/001, "Handles").
- A call that can fail returns a `WErr` and fills a `WWhy` (code and
  sentence; `src/013-errors.h`).
- World changes come only from the server (docs/001, "Who holds the truth").
