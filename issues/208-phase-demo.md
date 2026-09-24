# 208 — Phase 2 Capstone Demo

## Current behavior

Phase 2 not started. No window, no on-screen pixels, no path from BLP to
visible texture.

## Intended behavior

A windowed application that exercises every Phase 2 subsystem:

- Opens a window (raylib, GL 3.3 core under the hood) via the new
  [window subsystem](./201-window-and-context.md).
- Loads a small handful of known BLPs from the live MPQ chain (Phase 1)
  and uploads them as `gfx_texture_t` via [204](./204-blp-to-gpu-upload.md).
- Renders them through the [batched sprite renderer](./205-sprite-renderer.md),
  with mouse drag panning + wheel zooming the
  [2D camera](./207-camera.md).
- Loads the Blizzard `FRIZQT__` font from the MPQ chain via the
  [text subsystem](./206-bitmap-text.md) and overlays per-sprite
  labels.
- Displays an on-screen HUD (drawn with raylib's `DrawText` + the debug
  font) reporting **stats per CLAUDE.md** ("phase demos focus on stats
  and datapoints, not narration"):
  - Frame time (ms) + FPS.
  - Sprites this frame.
  - Sprite batches this frame + batch-break causes.
  - Texture memory in use (live + cached).
  - Shader cache size.
  - BLP load count, total bytes streamed from MPQs this session.
  - Driver: vendor, renderer, GL version, S3TC support yes/no.
  - Texture sources this session: pack / overlay / blizzard / placeholder
    (a test replacement pack is installed for the demo, so borrowed and
    replaced textures sit side by side, each labelled with its source).
- Hot reload demonstration: editing `shaders/textured.frag` and saving
  recolors all sprites on the next frame.

## Suggested implementation steps

1. Add a `demos/` subdirectory under `src/demos/phase-2/` (mirrors the
   Phase 1 demo layout).
2. Build it into a separate executable: `phase-2-demo`.
3. Write `issues/completed/demos/phase-2.sh` (hard-coded `${DIR}`,
   `--dir` override).
4. Extend `run-phase-demo.sh` at the project root so it dispatches to
   phase 2 when the user picks 2.
5. The demo's `main`:
   - Load config + log + window.
   - Open MPQ chain, load 4–6 known BLPs, load FRIZQT.
   - Init sprite renderer, init 2D camera at default zoom.
   - Main loop: input → camera update → begin frame → draw sprite grid
     → draw labels → draw HUD → end frame.
6. Print a final stat dump on quit + write a `goodbye.txt` to
   `output/` per CLAUDE.md ("the last thing a program should do is
   write to output/").

## Acceptance

- `./run-phase-demo.sh` → choose 2 → window opens, sprite grid visible,
  HUD updates each frame, pan/zoom feel correct.
- Editing the textured fragment shader live updates the sprites on the
  next frame.
- Closes cleanly on ESC or window close; ASan-clean; `output/goodbye.txt`
  written.

## Related

- All other Phase 2 issues are blockers (201–207).
- Becomes a permanent deliverable, per CLAUDE.md ("phase demos are
  part of the deliverable product").
