# 109 — Test Scenes: the Client's Half

*Came from: wow-chat-2026 173w (queued inputs and test characters),
moved 2026-10-07. The whole design, with the server's half (scenes set
up by test patches on the cc-test server profile), is wow-chat-2026
176a; its open questions are this issue's too.*

## Current behavior

The client is tested two ways, neither playing a sequence of inputs:
- **The test program** (`tests/044-test-client.c`, `scripts/build
  --test`) calls each part directly with made-up input: fast, no window,
  but it never runs the client's own frame loop.
- **Pictures** (`scripts/run --shot`) run the real loop with a window,
  steered by one-off options (`--walk`, `--dive`, `--wait`, `--goto`,
  `--walking`, `--pose`), one character, one picture after a fixed
  settling time; the run report is printed text, not a log a test reads.

What is ready for it: runs repeat exactly now (particles are seeded by
which copy and emitter they are, 403), and every part keeps its state
behind a handle (docs/001), so several worlds and characters can live in
one program.

## Intended behavior

The owner, 2026-10-07: "we should maybe create a plan to have a series of
'queue'd inputs' that can be provided to a character when testing? Then,
we can create several 'test characters' at once, and unless we're testing
rendering, we should be fine to test multiple things at once, if they
output logs correctly."

- **A queue of inputs per character:** moments of world-time and what
  happens at each: an action pressed or released (the original's action
  names: MOVEFORWARD, JUMP ...), a picture taken, a log line written.
  Played at a fixed step (20 frames a world-second), so a run repeats.
- **The picture options become queues:** one way to steer the loop.
- **Logs a test reads:** per character, in RAM
  (`tmp/shared-memory/test-scenes/<scene>/<character>.log`): world-time,
  position, facing, on the ground or swimming or falling, the animation
  and how far into it, what is held.
- **Many characters without a window:** the world's shape (ground,
  walls, water) loaded once, many movers and animation choosers, each
  with its queue and log.
- **Pictures without a window:** a scene drawn straight into a picture
  file, so hundreds can be taken in one run.
- **Against a server:** the same queues driving logged-in test
  characters on the cc-test profile (with Phase 5).

## Suggested implementation steps

1. The queue reader, refusing bad lines by number (as the settings file).
2. The frame loop taking held actions from a queue; the picture options
   rewritten as queues; reference pictures compared before and after.
3. The log.
4. The windowless mode with many characters; a test running two at once.
5. The first scenes: running into a wall keeps the run playing; walking
   halves the stride; a jump's start, loop and landing in order.
6. Pictures without a window.

## Open questions

Kept together in wow-chat-2026 176a (the queue's format; who plays the
characters; whether characters in one run meet; who writes the log; how
scenes are kept apart; whether the picture options stay as shortcuts).

## Related

- wow-chat-2026 176 (the cc-test profile) and 176a (test scenes).
- [403](./403-particles.md) (runs repeat), [101](./101-build-system.md).
