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

## Open questions

1. **When nothing is happening.** If every worker is idle, none is
   taking tasks, so none reads the clock, and a due entry waits until
   some task arrives. Something must notice the moment. Candidates:
   (a) the window's render thread, awake every frame anyway, also reads
   the clock (one comparison a frame; lateness at most one frame);
   (b) the last worker to go idle sleeps only until the earliest due
   moment (a sleep with a deadline, so not a spin and not an open-ended
   wait); (c) both. Which?
2. **The registry's shape.** A timing wheel (a ring of buckets, one per
   increment, an entry put in the bucket of its moment; inserting and
   taking due entries cost the same whatever the count) or a sorted list
   (simplest; inserting costs more as it grows)?
3. **The increment.** One millisecond, the simulation's tick (16 ms), or
   per entry?

## Suggested implementation steps

To be written once the shape is chosen.

## Related

- [110e](./110e-per-frame-work-as-stations.md) (standing tasks), Phase 5
  (the network session); world-edit-to-execute 401b (its timer subsystem).
