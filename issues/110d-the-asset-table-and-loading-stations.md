# 110d — The Asset Table and Loading as Stations

*Parent: [110](./110-the-ceramic-shape.md).*

## Current behavior

*Paused with its parent, 2026-10-07: blocked until soramech has timers
and standing tasks ([110](./110-the-ceramic-shape.md)).*

Loading is split between the main thread (tiles in reach: reading,
preparing, sending to the card) and the reading-ahead thread
(`src/034-prefetch.c`: the next ring of tiles unpacked and their ground
prepared ahead). They share the archive reader's memory and the
server's heights under locks. Models, pictures and buildings are held
by the drawing parts that loaded them.

## Intended behavior

**One append-only asset table** (515e) holds everything large and
read-only: meshes, pictures, bind poses, animation tracks, prepared
ground. Stations and mailboxes carry a small number naming an entry,
never a copy. The table grows while the graph runs; entries never change
or move (it grows in blocks, never reallocated); an entry is published
by one atomic count increase after it is complete.

**Loading is stations**: which tiles are wanted (from where the camera
and the player are), reading a file out of the archives, decoding a
model or picture, preparing a tile's ground. Many run at once on the
workers. Sending to the card is a render thread's: between frames it
uploads entries finished since the last frame and marks them drawable.

The reading-ahead thread (034) retires: its work is these stations,
asked for further out.

## Suggested implementation steps

1. Port 515e's table; tests: entries published only when whole, readers
   on many threads during growth (`--tsan`).
2. Stations for reading, decoding (pictures 017, models 022, ground
   033), each writing an entry; the archive chain's lock stays (015).
3. The wanted-tiles station, replacing 034's ring and 043's tile policy.
4. Uploading on the render thread: newly finished entries, a budget of
   milliseconds per frame so a burst of loads never stalls a frame.
5. Retire 034; rewrite 303's Current behavior.

## Acceptance

- Walking across tile edges in Ashenvale: no frame over the frame budget
  from loading (frame-time histogram), with 034 gone.
- The race-checked build clean while loading.

## Related

- world-edit-to-execute 515e; [303](./303-wdt-and-streaming.md), [105](./105-mpq-reader.md).

## Blocks

- [606](./606-the-loading-screen.md) (waiting for every read to finish).
