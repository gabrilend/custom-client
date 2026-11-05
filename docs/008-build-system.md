# 008 — Build System

One bash script calls the compiler directly. "bash is easier for me. We
can always call cmake inside of bash" (owner, 2026-10-07). Rewritten
2026-10-07 from the earlier CMake plan; the shape below is the working
build of wow-chat-2026's custom client, carried over in the merge
(wow-chat-2026 issue 173v).

Per the house rules every script has a hard-coded `${DIR}` (the project
folder) at the top, takes another as its first argument, and keeps every
path relative to `${DIR}`.

## Entry points

| Script | What it does |
|--------|--------------|
| `scripts/build` | Compiles the client, its test program and the shared reader library. Options: `--test` runs the tests after building; `--asan` also builds the address-checked client; `--tsan` also builds the race-checked client. Prints how long the build took and appends it to `tmp/shared-memory/build-times.log`. |
| `scripts/run` | Runs the client, rebuilding it first when any source is newer than the program. `--shot` takes a picture and quits (the picture options are in the script's header). |
| `scripts/generate-info` | Writes each module's `.info.md` from its header's comments. |

## What one build makes

| Product | From | Used by |
|---------|------|---------|
| the client (`w-client`) | every module and `main` | players, pictures |
| the test program (`w-client-test`) | every module that needs no window, and the tests | `scripts/build --test` |
| the reader library (`libwreaders.so`) and its header (`wreaders.h`) | the window-free readers: archives, tables, pictures, models, ground files, buildings | world-edit-to-execute, from Lua through LuaJIT's FFI ([001](./001-architecture-overview.md)) |
| `w-client-asan`, `w-client-tsan` | the client, with clang's checkers | hunting memory faults and races |

Modules are split by whether they need a window: the readers and the
game's arithmetic (archives, tables, pictures, models, ground preparation,
the mover, collision, the settings store) build without raylib's window,
so the test program and the library link them alone. Drawing modules link
raylib.

## Output layout

```
${DIR}/
  tmp/            # symlink → /tmp/azc-custom-client/ (RAM; the exec tier)
    tmp/build/    #   the programs and the library (they are executed)
    shared-memory/  # symlink → /dev/shm/custom-client/ (RAM; logs, pictures, timings)
```

`tmp/` follows the house two-tier layout, kept by
`/home/ritz/programming/ai-stuff/scripts/libs/ensure-ram-tiers`:
executable things in `/tmp/<name>/tmp/`, everything only read or written
(logs, pictures, dumps) in `/dev/shm/<name>`, reached as
`tmp/shared-memory/`. Both live in RAM, so a reboot empties them; the
link survives, and scripts call `ensure_ram_tiers` before writing.

Nothing is written into `${DIR}/` outside `tmp/`. Git ignores it.

## Compile flags

```
-std=c11 -D_FILE_OFFSET_BITS=64 -D_POSIX_C_SOURCE=200809L -O2 -g
-Wall -Wextra -Werror
```

The checked builds use clang with `-fsanitize=address` or
`-fsanitize=thread`, `-O1 -g`.

## Dependencies

- **raylib** 5.x, static, from `/usr/local` today. Planned: from `libs/`
  when present, otherwise fetched once into `libs/` and built there, so a
  fresh machine builds the same (carried over from wow-chat-2026 issue
  173k, not built yet).
- **zlib**, **pthreads**, **OpenSSL** (with networking), **LuaJIT** (with
  scripting): the system's.

## The five-minute rule

Claude may run this build. If a build ever takes more than about five
minutes, it stops building and hands the build to the owner, as with
every long build (the rule carried over from wow-chat-2026's CLAUDE.md).
The script prints a warning when that line is crossed.

## Tests

One test program, run by `scripts/build --test`. Each check prints `ok:`
or `FAIL:` with what it measured, and the program ends with the count.
A check that needs what only the owner's machine has (the client's
install, the server's extracted files under `assets/server-data`,
war3.mpq) and doesn't find it prints `NEEDS:` with what is missing. It
is counted apart in the closing line, and the program exits 2 rather
than 0. One that finds the file and gets a wrong answer is still a
`FAIL:` (exit 1). A run on the owner's machine should show no NEEDS
(issue 1109 step 1x).
The checks hold the client to facts about the original game's own files
(Astranaar's ground height, her run speed, the server's own placement of
every doodad) and to measured behaviour (tile loads, pictures compared
pixel by pixel). No test framework.

## Related

- [002-technology-stack.md](./002-technology-stack.md): what is built.
- [009-roadmap.md](./009-roadmap.md): the build is the first Phase 1
  issue (101).
