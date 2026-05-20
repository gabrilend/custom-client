# 108 — Phase 1 Capstone Demo

## Current behavior

No phase 1 demo exists.

## Intended behavior

A small program (`issues/completed/demos/phase-1.sh` plus the binary it
runs) that exercises everything Phase 1 built, end to end, and reports
visible stats. Per CLAUDE.md, phase demos focus on **stats and
datapoints**, not on narrating the implementation.

The demo:

1. Loads `client.ini` from `${DIR}/input/`.
2. Opens the configured MPQ chain.
3. Iterates the chain and prints: total archives, total file count,
   override-file count (files present in more than one archive).
4. Loads `DBFilesClient/Map.dbc` and prints: row count, the names of all
   continents, the names of all five-player dungeons.
5. Loads `DBFilesClient/ChrRaces.dbc` and prints: each race id, name,
   faction.
6. Decodes a known BLP (login-screen background) and writes a `.tga` to
   `tmp/phase-1-demo/`. Prints the BLP's format, dimensions, mip count.
7. Prints a summary line: archives opened, DBC rows read, BLPs decoded,
   bytes streamed.

A short bash script under `issues/completed/demos/phase-1.sh` runs the
demo against the configured asset root. The top-level script
`run-phase-demo.sh` asks for a number and dispatches to the corresponding
phase demo.

## Suggested implementation steps

1. Add a `demos/` subsystem under `src/demos/phase-1/`.
2. Build it into a separate executable: `phase-1-demo`.
3. Write `issues/completed/demos/phase-1.sh` (hard-coded `${DIR}` per
   convention, `--dir` override, runs the executable).
4. Write `run-phase-demo.sh` at the project root that prompts for a
   phase number (1..N where N is the number of completed phases) and
   execs the matching demo script.
5. Document what each printed number means inline as a final printed
   "legend".

## Acceptance

- `./run-phase-demo.sh` → choose 1 → demo runs and prints stats.
- `tmp/phase-1-demo/<name>.tga` is openable in any image viewer and
  shows the expected texture.
- The demo exits 0 on a healthy chain; exits non-zero with a clear log
  line on any error.

## Related

- All other Phase 1 issues are blockers.
- Becomes a permanent deliverable, per CLAUDE.md ("phase demos are part
  of the deliverable product").
