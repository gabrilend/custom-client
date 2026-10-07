# 303 — WDT Parser and Tile Streaming Policy

*Filled in part by the code from wow-chat-2026 (its 173s, 173u), 2026-10-07.*

## Current behavior

Built: tiles loaded round her by the view distance, dropped a tile
beyond (`043-main.c`); a second thread (`src/034-prefetch.c`, a handle)
reads the next ring's files ahead and prepares their ground; creatures
built as they come into sight; the far horizon from the low-detail map
(`src/039-horizon.c`). A walking tile load stalls about 14 ms (docs/047).
Checked: a far jump leaves exactly what starting there gives.

## Intended behavior

**Since 2026-10-07:** the streamer thread below becomes loading stations
on the ceramic engine, and the reading-ahead thread (034) retires into
them ([110d](./110d-the-asset-table-and-loading-stations.md)). The
choreography below is the earlier design.


Two intertwined deliverables.

### The WDT parser

```c
err_t wdt_load(mpq_chain_t *, const char *map_internal_name, wdt_t **out);
void  wdt_free(wdt_t *);

bool         wdt_tile_exists(const wdt_t *, int x, int y);  /* 0..63 */
bool         wdt_is_global_wmo(const wdt_t *);
const char  *wdt_global_wmo_path(const wdt_t *);            /* iff global */
```

The WDT for a continent is a small file with three chunks:

- **MVER** — version.
- **MPHD** — flags. One flag says "this map is a single global WMO (a
  dungeon, raid, or instance) — load that instead of an ADT grid."
- **MAIN** — 64×64 bitmap of "does the ADT at (x,y) exist."

For the global-WMO case, the file also carries MWMO + MODF chunks
pointing at the one WMO. Most outdoor maps (Azeroth, Kalimdor,
Northrend) take the bitmap path; most dungeons take the global-WMO
path.

### The streaming policy

A subsystem that owns:

- A **streamer thread** that pulls work off a request queue, calls
  [`adt_load`](./301-adt-parser.md), and pushes the parsed `adt_t` onto
  a results queue.
- A **policy** on the main thread that, each frame, decides which tiles
  should be resident given the current camera position and a
  configurable view radius (default 3 — i.e. a 7×7 window centered on
  the camera's tile).
- An **eviction** rule: tiles outside the window for more than N frames
  get freed (CPU side) and their GPU resources released (deferred to
  the main thread on the next frame).

```c
typedef struct world_stream world_stream_t;

err_t world_stream_init  (mpq_chain_t *, const wdt_t *, world_stream_t **out);
void  world_stream_close (world_stream_t *);

void  world_stream_set_camera_tile(world_stream_t *, int tx, int ty);

/* Called once per frame on the main thread; drains the results queue,
 * issues GPU uploads, evicts stale tiles. */
void  world_stream_tick(world_stream_t *, terrain_renderer_t *);

int   world_stream_resident_tile_count(const world_stream_t *);
const adt_t *world_stream_tile(const world_stream_t *, int tx, int ty);
```

### Cross-thread choreography

This is the first place we lean on the streamer-thread pattern hinted
at in [docs/001](../docs/001-architecture-overview.md):

```
main thread                            streamer thread
-----------                            ---------------
update camera_tile
build want-set (within radius)
diff against resident set
push (load tx,ty) requests   ─────►   pop request
                                       fs_read 3 ADT files
                                       parse → adt_t
                              ◄─────  push result
drain result queue (per frame)
  → terrain_renderer_upload
push (unload tx,ty) for stale
  → free adt_t + handle
```

The queues are SPSC rings as documented in the
[strategems](../strategems/patterns-noticed.md). GPU upload happens
only on the main thread (the
[201 main-thread guard](./201-window-and-context.md) catches violations).

## Suggested implementation steps

1. Create `src/wdt/` subsystem; implement the parser.
2. Create `src/world_stream/` subsystem.
3. Implement the streamer thread:
   - Single producer (main thread enqueueing requests).
   - Single consumer (streamer thread popping).
   - Work item: `{tx, ty, kind: load|unload, generation_id}`.
   - The streamer thread loops: pop, do I/O + parse, push result.
4. Implement the per-frame policy on the main thread:
   - Compute the want-set from camera tile + radius.
   - Diff against the resident set; enqueue missing.
   - Mark tiles that fell out of the window with their "last seen"
     frame number; evict after a grace period (configurable, default
     30 frames ≈ 1 second at 30 Hz).
5. Drain the result queue once per frame; for each parsed `adt_t`,
   call `terrain_renderer_upload` and store the handle.
6. Add stats: tiles resident, tiles in flight (requested but not yet
   delivered), bytes streamed this frame, GPU memory used by terrain.
7. Tests:
   - WDT parser against a fixture file.
   - Streamer thread under load — feed it 50 requests and assert all
     are delivered in order; assert no leaks (ASan).
   - Policy: simulate a camera moving across the tile grid for 1,000
     ticks and assert the resident set tracks correctly.

## Acceptance

- Loading the WDT for `Azeroth` and `Kalimdor` reports plausible tile
  counts (the bitmap is mostly sparse).
- The Phase 3 demo seamlessly streams tiles as the camera moves — no
  hitches on the main thread (verified by frame-time histogram in the
  HUD).
- ASan-clean across a 5-minute fly-around session.

## Stress markers

This issue is where the project first runs C with real threading. The
ASCII diagram above is the only kind of "doc beside the code" that
keeps the choreography legible — the actual code is hard to read
sequentially because pieces of it live on different threads. Put the
diagram in a header comment in `world_stream.c` and update it whenever
the choreography changes.

The eviction grace period exists because the camera can wobble across a
tile boundary; without it, a tile would unload-then-reload every time
the camera grazed its edge. The default 30 frames is a guess — tune it
once the demo exists and we can feel a thrash.

## Related

- [301](./301-adt-parser.md)
- [302](./302-terrain-renderer.md)
- [strategems/patterns-noticed.md](../strategems/patterns-noticed.md)
  (SPSC ring strategem)
- Blocked by: 301, 302.
- Blocks: [308](./308-phase-demo.md).
