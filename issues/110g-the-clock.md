# 110g — The Clock: Work at Times, with No Thread Asleep Waiting for Them

*Parent: [110](./110-the-ceramic-shape.md). Design still open: the shape
below is the owner's sketch of 2026-10-07 and the questions it raises.*

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

## Leaning (2026-10-07, not yet settled)

The owner considered a task that hands itself in again each time it
finishes, to check the clock, and noticed it "would certainly spin when
nothing else is going on", then: "maybe the render thread is the right
idea, with the 'ring of buckets'". So the leaning is:

- **The render thread moves the clock's hand** once a frame (it is awake
  every frame anyway); any worker taking a task may also move it, which
  makes the clock finer while the client is busy. Lateness while idle:
  at most one frame (about 16 ms at 60 a second); on average half that.
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

## Open questions

1. Is a frame's lateness acceptable for everything the clock serves? The
   socket polled once a frame adds up to 16 ms before a server message
   is seen.
2. The increment: one millisecond, or the frame?

## Suggested implementation steps

To be written once the shape is chosen.

## Related

- [110e](./110e-per-frame-work-as-stations.md) (standing tasks), Phase 5
  (the network session); world-edit-to-execute 401b (its timer subsystem).
