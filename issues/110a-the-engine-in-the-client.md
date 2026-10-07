# 110a — The Ceramic Engine in the Client

*Parent: [110](./110-the-ceramic-shape.md).*

## Current behavior

The client has no worker pool. The engine exists as world-edit-to-execute's
kept copy (`src/render/ceramic/engine/cera.c`, `cera.h`, about 11,900
lines; started from soramech `059487d`, every change marked `FORK` and
listed in its `fork-notes.md`): a lock-free task ring, batched hand-in,
waking only sleeping workers, results counted after they land, and
several task queues ("destinations") a station may name. Its tests are
`src/render/ceramic/engine/tests/`.

## Intended behavior

The engine is part of this client's source, numbered in reading order
like every other part, built by `scripts/build` into the client and the
test program, with its own tests run by `scripts/build --test`. A first
map (a readable text file naming stations, the box each runs and how
their ports connect) runs one station headless from the test program,
so the engine is proven inside the client before anything depends on it.

## Suggested implementation steps

1. Take the next numbers from `.file-index-counter` for the engine
   (`cera.c`, `cera.h`) and its `.info.md`; keep `fork-notes.md` beside it,
   naming world-edit-to-execute's commit it was taken from.
2. `scripts/build`: the engine joins CORE (no window); the build time is
   watched (the five-minute rule, docs/008).
3. Port the engine's tests (task queue, destinations) into the test
   program, or run them as their own small programs from `--test`.
4. A first map with one box (sum a buffer in chunks) delivered from the
   test program and collected; checked under `--tsan`.

## Acceptance

- `scripts/build --test` builds the engine and passes its tests.
- The race-checked build runs the first map with no report.

## Related

- world-edit-to-execute 515g (the lock-free ring), 515i (destinations).
- Parent's open question 1 (where the kept copy lives).
