# 606 — The Loading Screen

*Written 2026-10-16 from the clean-room checklist's note 10.3
([docs/057](../docs/057-what-the-original-program-decides.md)), at the
number Phase 6's capstone sketched for "world enter sequence (loading
screen, terrain pre-stream)".*

## Blocked by

- [303](./303-wdt-and-streaming.md) (loading the world around a point),
  [110d](./110d-the-asset-table-and-loading-stations.md) (file reads
  that can be waited on).

## Blocks

- [608](./608-phase-demo.md).

## Current behavior

The client loads the world around the player while drawing it; there's
no loading screen and no bar.

## Intended behavior

Entering the world and teleporting show a loading screen that ends and
fills as the original's does (note 10.3):

- **Ends** only when every file read for the terrain, buildings and
  doodads around the arrival point has finished.
- **The world part of the bar** moves in four rounds of setting up nearby
  tiles, buildings and doodads and waiting for their reads: 0.25, 0.5,
  0.66, 0.75, then 1.0 after a last full wait, so it jumps from three
  quarters to full at the end (a **quirk**, below).
- **The whole bar:** on the first entry, half is the interface's files
  loaded out of the total and half is the world; on a teleport the world
  fills it all.
- **While it's up:** input swallowed; screen, sound and bar refreshed at
  most every 250 ms; a keep-alive to the server every 30 s; the tip line
  in light grey (about 78% grey, 84% opaque), 18/1000 of the screen high,
  with a black shadow offset 1/1000, trailing line breaks removed.
- **Streaming mode** (a trial still downloading data) fills by its own
  schedule and race-data rule; this client has no such download, so it's
  noted here and not built.

### Quirks, each with a correction switch (off by default: the original)

| Quirk (10.3) | The original | The correction |
|---|---|---|
| the bar's schedule | fixed steps 0.25, 0.5, 0.66, 0.75, 1.0, not a measure of work | fill by the share of file reads finished |
| streaming weight | file streaming is tracked but counts 0 in the bar | (no streaming mode here, so nothing to correct) |

## Suggested implementation steps

1. The four rounds and the final wait, with 303's loading and 110d's
   reads.
2. The bar's halves (interface and world).
3. The screen itself: picture, tip, bar, input swallowed, the 250 ms
   refresh, the 30 s keep-alive.
4. Tests (window-free, with reads that finish on a scripted clock):
   - the bar reads 0.75 until the last read finishes, then 1.0;
   - with the correction on, it reads the share of reads done;
   - a teleport's bar is all world;
   - a load lasting 70 s sends two keep-alives.

## Open questions

None.

## Related

- [docs/057](../docs/057-what-the-original-program-decides.md) note 10.3,
  [303](./303-wdt-and-streaming.md).
