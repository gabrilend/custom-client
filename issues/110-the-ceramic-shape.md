# 110 — The Ceramic Shape: No Main Loop, Only Render Threads

*Came from: world-edit-to-execute issue 515 (the render graph on the
ceramic engine, its sub-issues 515a to 515k) and the owner's decision of
2026-10-07: "I much prefer the ceramic engine to single threading ... I'd
prefer not to have any main thread at all, or if we do, that it's only
the rendering thread. Ideally we'd be able to have multiple rendering
threads for different openGL contexts, so that we can do things like
'picture in picture' or whatever."*

## Current behavior

The client has two threads (docs/001, "Threads"):

- **The main thread** (`src/043-main.c`) does everything each frame, one
  step after another: reads input, steps the mover, loads tiles, poses
  every animated model's bones (`042-doodads.c`, `041-figure.c`), steps
  every particle emitter, decides what is in view, runs Lua, and makes
  every OpenGL call.
- **One reading-ahead thread** (`src/034-prefetch.c`) unpacks the next
  ring of tiles' files and prepares their ground.

So all per-frame work runs on one core. With a few dozen figures in view
(the WoW world) that holds; a WC3 map with hundreds of units, each posed
from its own skeleton every frame, is batch work on a single thread,
which the house rules forbid.

## Intended behavior

The client is a **ceramic program**: C functions ("boxes") wired into
stations by a readable map file, run by the ceramic engine (soramech's
`cera.c`, in the kept copy world-edit-to-execute improved in 515g and
515i). A station runs on any worker the moment every one of its inputs
holds a value. There is no frame loop that does the work.

The only threads that are not workers are **render threads**. A render
thread owns exactly one OpenGL context and does only:

1. take the newest finished state for its view out of its mailbox
   (110c) without waiting;
2. send anything newly finished in the asset table to the graphics card
   (110d);
3. draw that state, working out "now" from it (extrapolation, the
   camera and cursor at the last moment);
4. hand the frame's input events (the window's render thread only) into
   the graph as arguments.

This holds two facts of the hardware and libraries, which the design
works with rather than against:

- **An OpenGL context is current on one thread at a time**, and every
  call for it must come from that thread. So drawing is never a station:
  a station may run on any worker.
- **Window events are read on the thread that made the window** (GLFW,
  under raylib, documents this as the process's first thread). So the
  process's first thread becomes the window's render thread, and it
  reads events and hands them in; it does no game work.

Further views (a second view of the world, pictures for previews and
comparisons, the map browser's page) are drawn by the one render thread
into textures (110f; the owner, 2026-10-07: "I guess displaying it on a
texture is fine"). More render threads stay possible later if a
measurement asks for them.

Work that must happen at a time (polling the server's socket, ticks,
timers) is handed in by a clock (110g), so there is no network thread
and no timer thread. Per-frame work runs as **standing tasks**: a task
that stays in existence and is handed in again each tick, writing only
its own slice, skipped for a tick while still running (a flag says so).
The owner: "Let's build it now and see what happens. We can optimize it
later." Built here (110e), and offered to soramech after.

Everything else is a station: loading and decoding files, preparing
ground, posing models, stepping particles, deciding what is in view,
the mover, Lua scripts, decoding the server's packets.

## Sub-issues

| ID | Name | Dependencies | Description |
|----|------|--------------|-------------|
| 110a | [the-engine-in-the-client](./110a-the-engine-in-the-client.md) | 101 | The kept copy of the ceramic engine, numbered into `src/`, built and its tests run by `scripts/build`; a first map that runs a station headless |
| 110b | [the-window-render-thread](./110b-the-window-render-thread.md) | 110a, 201 | The process's first thread owns the window and its context: events in, newest state out, draw; the guard that every GL call comes from a render thread |
| 110c | [the-mailbox](./110c-the-mailbox.md) | 110a | The three-buffer mailbox per view (515c): stations write, a render thread takes the newest, neither waits; extrapolation and snapping corrections (515d) |
| 110d | [the-asset-table-and-loading-stations](./110d-the-asset-table-and-loading-stations.md) | 110a, 110b | The append-only asset table (515e); reading tiles, models and pictures as stations; the reading-ahead thread (034) retired into them |
| 110e | [per-frame-work-as-stations](./110e-per-frame-work-as-stations.md) | 110c, 110d | Posing, particles, what is in view and the mover as stations in chunks, writing the mailbox; measured against the single-thread frame |
| 110f | [views-drawn-into-textures](./110f-views-drawn-into-textures.md) | 110b, 110d | A second view, pictures for files, and the map browser's page, each drawn into a texture by the one render thread |
| 110g | [the-clock](./110g-the-clock.md) | 110a | Work at given moments (socket polling, ticks, timers) with no thread asleep waiting; design open |

Execution order: 110a → (110b ∥ 110c ∥ 110g) → 110d → 110e → 110f

## Suggested implementation steps

1. 110a: the engine in the build, alone, with its own tests.
2. 110b and 110c together: the window thread and the mailbox it reads.
3. 110d: loading as stations; this retires 034's thread.
4. 110e: the frame's work as stations; the frame-time comparison.
5. 110f: views into textures. 110g once its shape is chosen.
6. Rewrite docs/001 "Threads" and docs/005 "The frame" to the built shape.

## Acceptance

- No thread other than workers and render threads exists in a running
  client (checked by listing the process's threads in a test).
- The race-checked build (`--tsan`) runs a scene with no report.
- A crowd of 500 WC3-sized figures, posed and drawn: frame time and
  render-thread time recorded beside the single-thread frame's, in this
  issue.
- A second view and a picture for a file drawn into textures while the
  window draws, in one run.

## Open questions

1. ~~Where the kept engine lives~~ Answered 2026-10-07: nowhere as a
   copy. soramech (`minimal-soramech`) is the upstream, pinned and
   cloned at build time, and world-edit-to-execute's improvements are
   patches applied on top (110a; the owner: "we should use the patch
   system to apply patches when needed, and keep the upstream clean").
2. ~~The network thread~~ Answered 2026-10-07: none; the socket is read
   without waiting by a task the clock hands in (110g).
3. ~~Standing tasks~~ Answered 2026-10-07: built now, here (110e).

## Related

- world-edit-to-execute `issues/515-render-graph-on-the-ceramic-engine.md`
  and its sub-issues (the measurements this design rests on).
- [201](./201-window-and-context.md) (its main-thread guard becomes the
  render-thread guard), [303](./303-wdt-and-streaming.md) (its streamer
  thread becomes stations), [402](./402-skeletons-and-animation.md),
  [403](./403-particles.md) (posing and particles as stations),
  [109](./109-test-scenes-client-half.md) (pictures without a window).
- docs/001 "Threads", docs/005 "The frame".
