# 201 — Window and Render Context

*Filled in part by the code from wow-chat-2026 (its 173a, 173k, 173b5), 2026-10-07.*

## Current behavior

Built (`src/043-main.c`): a raylib window at the settings' resolution,
windowed or fullscreen, maximized or not, vsync and resizing live; the
frame loop; Escape quits until the game menu exists; the window's size
kept when resized. The program's whole state is one App structure;
shutting down frees everything on the graphics card before the window
closes.

## Intended behavior

A dedicated subsystem owns the window, the raylib lifecycle, and the
canonical "is the application still running" flag. Everything else takes
a handle to it.

```c
typedef struct window window_t;

err_t window_init   (const window_cfg_t *cfg, window_t **out);
void  window_close  (window_t *);

bool  window_should_close(const window_t *);
void  window_begin_frame (window_t *);   /* BeginDrawing + ClearBackground */
void  window_end_frame   (window_t *);   /* EndDrawing                     */

int   window_width (const window_t *);
int   window_height(const window_t *);
double window_dt   (const window_t *);   /* seconds since last frame      */
```

**Since 2026-10-07 ([110](./110-the-ceramic-shape.md)):** there is no main
loop; the process's first thread is the window's render thread
([110b](./110b-the-window-render-thread.md)), and the guard below is a
**render-thread guard** per context.

The window subsystem also owns the **main-thread guard**: every other
subsystem that calls raylib or `rlgl` asserts that it's on this thread.
Catches threading mistakes at the boundary instead of as mysterious GL
errors later.

`window_cfg_t` is populated from the loaded config (issue
[103](./103-config-loader.md)) — width, height, vsync, fullscreen, MSAA
samples, target FPS, window title.

The main loop lives in `main.c`, not inside the window subsystem. The
window is just the thing the loop drives. The loop shape is:

```c
while (!window_should_close(w)) {
    window_begin_frame(w);
    /* phase 2: sprite + text from the demos */
    /* phase 6+: full scene */
    window_end_frame(w);
}
```

## Suggested implementation steps

1. Create `src/window/` subsystem (per CLAUDE.md, source files inside
   are indexed; first one becomes `012-window.c` / `013-window.h` —
   bump `.file-index-counter`).
2. Define `window_cfg_t` and `window_t`. The struct is opaque to
   callers; `window.c` knows the layout.
3. Implement `window_init`: `SetConfigFlags(FLAG_VSYNC_HINT | …)`,
   `InitWindow(w, h, title)`, `SetTargetFPS(target)`. Store the
   creating thread id for the main-thread guard.
4. Implement `window_close`: `CloseWindow()`, free the struct.
5. Implement `window_begin_frame`: assert main thread, `BeginDrawing()`,
   `ClearBackground(BLACK)` (or a configured clear color), record start
   timestamp.
6. Implement `window_end_frame`: assert main thread, `EndDrawing()`,
   compute `dt`.
7. Wire `main.c` to: parse args, load config, init log, init window,
   spin the demo loop, tear down in reverse order.
8. Tests: hard to test a window headlessly — settle for testing
   `window_cfg_t` validation (out-of-range dimensions error cleanly,
   contradictory flags error cleanly).

## Acceptance

- `scripts/run.sh` opens a black window at the configured resolution.
- ESC or window-close cleanly tears down: `CloseWindow` runs, the
  process exits 0, ASan is clean.
- Resizing the window updates `window_width` / `window_height` on the
  next frame (raylib reports this through `GetScreenWidth` /
  `GetScreenHeight`).

## Stress markers

raylib auto-creates the GL context for us, which is great for getting to
"window opens" fast but hides the moment that's most useful to assert on
(GL version, extension availability). Worth a one-line `TraceLog` of
what raylib reports for `rlGetVersion` at boot so we know early when
we're on a driver that's quietly downgraded us.

## Related

- [docs/005-rendering-pipeline.md](../docs/005-rendering-pipeline.md)
- Blocked by: [103](./103-config-loader.md), [102](./102-logging-and-errors.md).
- Blocks: every subsequent Phase 2 issue.
