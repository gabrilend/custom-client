# 110g — The Clock: Work at Times, with No Thread Asleep Waiting for Them

*Parent: [110](./110-the-ceramic-shape.md). Shape settled with the owner
2026-10-07 (the sketch, the leaning and the answers below).*

## Current behavior

*Paused with its parent, 2026-10-07: blocked until soramech has timers
and standing tasks ([110](./110-the-ceramic-shape.md)).*

Nothing in the client runs "at a time". The frame loop runs everything
every frame. The ceramic engine runs a station when its inputs arrive;
it has no notion of "at 12.5 seconds" or "every 50 ms".

## Intended behavior

**The clock belongs in the ceramic engine** (2026-10-07): the shape below
is to be built in soramech, so every ceramic program has it; this client
then uses it. The render thread moving the hand is this client's part.

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
  milliseconds, not every millisecond.
- **By refresh rate** (the owner asked about 144 and 240, 2026-10-07):

  | Rate | A frame | Buckets the hand passes a frame | Latest start | Average lateness |
  |------|---------|--------------------------------|--------------|------------------|
  | 60 Hz | 16.7 ms | 16 or 17 | 16.7 ms | 8.3 ms |
  | 144 Hz | 6.9 ms | 6 or 7 | 6.9 ms | 3.5 ms |
  | 240 Hz | 4.2 ms | 4 or 5 | 4.2 ms | 2.1 ms |

  The clock gets finer as the screen gets faster, at no cost: the hand
  moves the same way, through fewer buckets each time.
- **While the window is hidden** (2026-10-09) the render thread keeps
  looping at a floor rate, moving the hand and drawing nothing, so the
  world goes on in the background (answered question 4). Every entry is in
  fixed time; nothing fires "per frame" (answered question 3).
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

## What the original's clock does

From [docs/057](../docs/057-what-the-original-program-decides.md)
(filled 2026-10-16). These shape the clock's first users:

- **Time** is measured each frame in milliseconds from the system clock,
  and timers whose time has come fire first in the frame (note 11.1).
- **Movement** moves at most 250 ms at once; time beyond that in a long
  frame is skipped, and for the player's own unit reported to the
  server as a skip (note 2.1). So the mover's entry does **not** catch
  up a long frame bucket by bucket the way other entries do.
- **The interface's time-since-last-update** is the real time since the
  screen was last drawn. While the window can't be drawn it keeps adding
  up, so the first update after it returns receives the whole hidden
  time in one value: a **quirk**, built as written, with a correction
  switch (off by default). docs/057 suggests the correction start again
  from zero or a cap; this client's correction instead keeps the scripts
  turning while minimised (answer 6), which never builds a lump at all.
- **In the background** with background sleep on, the original sleeps
  between frames for its background-sleep time, in 25 ms steps, keeping
  sound running (note 1.4). This client's 20 turns a second while hidden
  (answer 5) is its own choice and stays one.

## Open questions

None.

## Answered

1. ~~Is a frame's lateness acceptable?~~ Yes (2026-10-07).
2. ~~The increment~~ One millisecond, the hand moved once a frame.
3. ~~"Every frame" as its own kind of entry?~~ No (2026-10-09). The owner:
   "We shouldn't attach it to the render speed. It should have a fixed
   value. I think... I think we might be able to attach it to the render
   frame IF we measure the absolute time that has passed. But that'll slow
   down every single frame". Every entry is in fixed time; the frame only
   moves the hand, by the absolute time measured, which is the shape
   above. The cost was measured on the owner's machine: one read of the
   monotonic clock (`clock_gettime`, answered by the kernel's shared page
   without entering the kernel) takes about 17 ns, 0.0004% of a 240 Hz
   frame. Drawing stays smooth at any rate because whatever draws a frame
   reads the time itself and places things where they are at that
   instant; the clock schedules work, it does not pace drawing.
4. ~~A window that stops drawing.~~ Keep turning (2026-10-09). The owner:
   "it should be processing in the background. I think option (a) is
   probably the best shape." While the window is hidden or minimized the
   whole world goes on, not only the connection: the render thread keeps
   looping at a floor rate, moving the hand and drawing nothing, so the
   clock, the socket polling and everything they hand in carry on. Chosen
   over the other shape offered, a kernel timer (Linux's `timerfd`) waking
   an idle worker to move the hand, which would give the hand two movers
   and need them never to move it at once.
5. ~~The floor rate~~ 20 turns a second while hidden (2026-10-09; the
   owner: "yeah 20 a second sounds... fine I guess?"): the hand then runs
   at most 50 ms late and the socket waits at most that long. A starting
   value, kept as one named constant beside the clock so it can be turned;
   any change goes in docs/011-balance-updates.md with its reason.
6. ~~The lump of minimised time~~ Built for parity, with a correction
   beside it (2026-10-16). First asked as "should interface scripts pause
   with drawing while the window is hidden?"; the owner: "that's... a
   different kind of 'hidden', not sure if I can answer that because the
   premises don't make sense." The premise came from the original's
   shape, where interface scripts run inside drawing; here they are their
   own work on the clock. Asked again as "should a switch recreate the
   lump?", the owner: "sure fine let's build it for parity first, but
   while we're doing so, add a patch file that we can enable or disable
   if we want to remove or fix that functionality." So:
   - **The original's behavior (the default):** while the window is
     minimised, the world keeps turning at the hidden floor rate, but the
     interface's update scripts are not handed in. Their elapsed time
     keeps adding up, and the first update after the window returns gets
     the whole minimised time as one value.
   - **The correction** (a switch the player can turn on, built at the
     same time, as docs/057 "The original's quirks" sets out): the
     interface's update scripts keep turning with everything else, each
     getting the small step since its last turn, so no lump arrives.
   - Both are tested: correction off, a 10-second minimise hands one
     update about 10 s; correction on, no update gets more than one
     hidden turn's time (about 50 ms).

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
