# 110g — The Clock: Work at Times, with No Thread Asleep Waiting for Them

*Parent: [110](./110-the-ceramic-shape.md). Shape settled with the owner
2026-10-07 (the sketch, the leaning and the answers below).*

## Current behavior

Nothing in the client runs "at a time". The frame loop runs everything
every frame. The ceramic engine runs a station when its inputs arrive;
it has no notion of "at 12.5 seconds" or "every 50 ms".

## Intended behavior

A **clock** keeps a registry of work to start at given moments: every
increment (a repeating entry), or once at a future increment and never
again. When a moment arrives, its work is handed into the graph as
ordinary tasks. Uses:

- **Polling the server's socket** (the owner, 2026-10-07: "something like
  a recurring timer which pulls from a network socket at a certain
  increment of time without spinning and sends its results (if any) to a
  box"). The socket is read without waiting (a non-blocking read returns
  at once, with the bytes that arrived or with "none yet"), so the
  polling task never sleeps; any packets go to the decoding box. This
  removes the need for a network thread.
- The simulation's fixed tick while offline (62.5 a second), WC3
  timers, the reading-ahead station's rounds, standing tasks (110e).

**The owner's sketch** (2026-10-07): "I'm picturing a timer that has a
registry of tasks to call every increment, or at certain specific future
increments and never again. We gotta do it without sleeping a thread
though. Maybe when we pick up a new task from a thread pool from any
thread we push the timer's current value to the list of things that are
expecting it at that time increment? It'd be dynamically adjusted as
time passes, things are recreated, etc... Not entirely sure about the
shape yet."

Read as a mechanism: there is no clock thread. Every worker, each time
it takes a task, first reads the clock and compares it with the
earliest moment in the registry (one atomic read and one comparison);
when that moment has passed, it takes the due entries and hands them in.
The checking rides on work that is happening anyway.

## The shape (settled 2026-10-07)

The owner considered a task that hands itself in again each time it
finishes, to check the clock, and noticed it "would certainly spin when
nothing else is going on", then: "maybe the render thread is the right
idea, with the 'ring of buckets'". So the leaning is:

- **The step is one millisecond; the hand moves once a frame** (the
  owner: "let's say one ms is the timestep, but we update the clock once
  per frame"). The window's render thread, awake every frame anyway,
  moves the hand from where it was to now, passing every millisecond
  bucket between in order (about 16 at 60 frames a second) and handing
  in what each holds. Workers do not move it. An entry's moment is kept
  to the millisecond, so work keeps its order and spacing, but it starts
  up to one frame late (about 16 ms; 8 on average).
- **A consequence:** a repeating entry with a period shorter than a frame
  fires several times in one burst each frame. Socket polling and other
  "every frame" work therefore repeat once per frame's worth of
  milliseconds (16), not every millisecond.
- **The registry is a timing wheel**: a ring of buckets like a clock
  face, one bucket per increment. Something due 5 increments from now
  goes into the bucket 5 past the hand. Each time the hand moves on a
  bucket, everything in that bucket is handed in. Putting an entry in
  costs the same however many entries exist; so does firing a bucket.
  Something due further off than one turn of the ring waits in a coarser
  wheel (as minutes wait for the hour hand) and drops into the fine one
  as its time approaches. Entries are added from any thread by pushing
  onto the bucket's list with one compare-and-swap, so adding takes no
  lock. A repeating entry puts itself back one period on when fired.

## Answered

1. ~~Is a frame's lateness acceptable?~~ Yes (2026-10-07).
2. ~~The increment~~ One millisecond, the hand moved once a frame.

## Suggested implementation steps

1. The fine wheel (a ring of millisecond buckets, a power of two, such
   as 1024) and a coarse wheel above it; each bucket a list pushed with
   one compare-and-swap; an entry holds its moment, its period (0 for
   once) and the task to hand in.
2. Moving the hand on the render thread: for each millisecond passed,
   take the bucket's list in one exchange, hand its due entries in as one
   batch, put repeating ones back one period on, and drop coarse entries
   into the fine wheel as their lap comes round.
3. Tests: entries fire in moment order; a once entry fires once; a
   repeating one keeps its period over many frames; entries added from
   many threads during a move all fire (`--tsan` clean); a frame that
   runs long fires every bucket it passed.
4. The first users: polling the socket without waiting (Phase 5), the
   offline simulation's tick, standing tasks (110e).

## Related

- [110e](./110e-per-frame-work-as-stations.md) (standing tasks), Phase 5
  (the network session); world-edit-to-execute 401b (its timer subsystem).
