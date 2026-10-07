# 110a — The Ceramic Engine in the Client

*Parent: [110](./110-the-ceramic-shape.md).*

## Current behavior

The client has no worker pool. Copies of the engine (2026-10-07):
soramech (`ai-playground/minimal-soramech`, the upstream, and
`tiny-soramech`), supcom-derivative-clone (`libs/900-cera.c`), and
world-edit-to-execute's improved copy.


The improved copy is world-edit-to-execute's
kept copy (`src/render/ceramic/engine/cera.c`, `cera.h`, about 11,900
lines; started from soramech `059487d`, every change marked `FORK` and
listed in its `fork-notes.md`): a lock-free task ring, batched hand-in,
waking only sleeping workers, results counted after they land, and
several task queues ("destinations") a station may name. Its tests are
`src/render/ceramic/engine/tests/`.

## Intended behavior

**The engine is an upstream, pinned and patched, never copied** (the
owner, 2026-10-07: "we should use the patch system to apply patches when
needed, and keep the upstream clean"). Upstream is soramech
(`/home/ritz/programming/ai-playground/minimal-soramech`, `src/cera.c`
and `src/cera.h`). Its engine files are unchanged since `059487d`, the
commit world-edit-to-execute's copy began from (checked 2026-10-07: ten
later commits, none touching the engine).

- **The clone is a build artifact.** `libs/soramech/` is cloned from
  upstream, git-ignored, reset to the pinned commit before every build,
  and never edited by hand. `.upstream-lock` holds the pin.
- **world-edit-to-execute's improvements become patches**, one per
  change in its `fork-notes.md` (about 860 changed lines in 59 places
  today): `patches/P001-...sh` onward. Each finds its spot by exact
  quoted text, wraps what it adds in `>>> P0nn <name> BEGIN` / `<<< ...
  END` markers, does nothing if its marker is already there, and can be
  taken back off exactly. Expected patches: results counted after they
  land; the lock-free task ring; batched hand-in; waking only sleepers;
  optional spinning; a full ring's behaviour; the fork's marker in the
  header; destinations. Standing tasks (110e) are a later patch.
- **A build** resets the clone to the pin, applies the patches (with a
  trap that takes them back off however the build ends), compiles the
  patched engine into the client, and takes the patches off again, so
  the clone is upstream exactly once more.
- **The machine's other parts:** `verify-patches` (each patch applies,
  applies again with no change, and comes off to the exact commit; then
  all together), a registry of patches generated from their headers, and
  `update-upstream` (moves the pin, auditing each patch against the new
  code: still fits, needs a look, upstream now has it, or no longer
  fits).
- When a patch has been taken up into soramech by its owner, the audit
  says so and the patch is retired.

A first map (a readable text file naming stations, the box each runs and
how their ports connect) runs one station headless from the test
program, so the engine is proven inside the client before anything
depends on it.

## Suggested implementation steps

1. The patch machinery from the house's upstream-patch-system (its
   shipped scripts copied into `scripts/`, with `DIR` set), a
   pre-build tier only.
2. Clone soramech into `libs/soramech/`, git-ignored; the first build
   from the checkout writes the pin.
3. Write the patches from world-edit-to-execute's `FORK`-marked changes,
   one at a time, each passing `verify-patches` before the next; at the
   end, the patched engine is compared with world-edit-to-execute's copy
   (they should differ only by the patch markers).
4. `scripts/build`: the patched engine joins CORE (no window); the build
   time is watched (the five-minute rule, docs/008).
5. The engine's tests (task queue, destinations) run from `--test`.
6. A first map with one box (sum a buffer in chunks) delivered from the
   test program and collected; checked under `--tsan`.

## Acceptance

- `scripts/build --test` resets, patches, builds, tests and unpatches;
  `git -C libs/soramech status` is clean afterwards.
- `verify-patches` passes every patch.
- The race-checked build runs the first map with no report.

## Related

- world-edit-to-execute 515g (the lock-free ring), 515i (destinations).
- world-edit-to-execute `src/render/ceramic/engine/fork-notes.md` (what each patch carries over), and its copy, frozen there as the record of 515.
