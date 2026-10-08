# 110b — The Window's Render Thread

*Parent: [110](./110-the-ceramic-shape.md).*

## Current behavior

*Paused with its parent, 2026-10-07: blocked until soramech has timers
and standing tasks ([110](./110-the-ceramic-shape.md)).*

`src/043-main.c` runs one loop on the process's first thread: it reads
input, does every part of the frame's work in order, and draws. Issue
201 plans a "main-thread guard" asserting that raylib is only called
from that thread.

## Intended behavior

The process's first thread becomes the **window's render thread**. It
starts the engine (110a) and the graph, opens the window (raylib, over
GLFW, which requires window creation and event reading on the first
thread), and then loops only:

1. read the window's events (keys, mouse, resize, close) and hand them
   into the graph as one argument, stamped with the time they were read;
2. take the newest finished state from its mailbox (110c);
3. send newly finished assets to the card (110d);
4. draw the state, with the camera and cursor worked out at this moment.

It never waits on the graph: with no new state it draws the last one
again, carried forward to "now". The guard of 201 becomes the
**render-thread guard**: each context remembers the thread it is current
on, and every drawing part asserts it is called from that thread.

## Suggested implementation steps

1. Split `043-main.c`: the window and drawing stay; every other step
   moves out as a station (110d, 110e) or into the input argument.
2. Input as data: one record per frame of events (what, when), delivered
   into the graph's input port.
3. The guard: a per-context owner thread, checked in debug builds by
   each drawing part's entry.
4. Shutting down: the window's close event delivered as an argument;
   the graph drains; the render thread frees the card's objects and
   closes the window last.

## Acceptance

- The first thread makes no call outside reading events, taking from the
  mailbox, uploading and drawing (checked by the guard and by review).
- A test feeds recorded input events and gets the same states a live run
  gets (the harness of world-edit-to-execute's bridge doc).

## Related

- [201](./201-window-and-context.md), [110c](./110c-the-mailbox.md).
