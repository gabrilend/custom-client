# 208 — Phase 2 Capstone Demo

## Current behavior

Phase 2 not started. No window, no GL context, no on-screen pixels.

## Intended behavior

A windowed application that:

- Opens an SDL3 window with a GL 3.3 core context.
- Loads a known BLP from the live MPQ chain (via Phase 1).
- Uploads it as a GL texture.
- Renders a textured quad that the user can pan + zoom with the mouse.
- Displays an on-screen HUD (drawn in C) with: frame time, draw calls,
  texture memory in use, BLP path.

## Suggested implementation steps

To be expanded into per-issue tickets (201–207) when Phase 2 begins.
Sketch:

- 201 — SDL3 window + GL context bring-up.
- 202 — Math primitives (vec2/3/4, mat4, quat).
- 203 — Shader pipeline (load, compile, link, hot-reload).
- 204 — BLP → GL texture upload (uses [107](./107-blp-parser.md)).
- 205 — Sprite / textured-quad renderer.
- 206 — On-screen text via a baked bitmap font.
- 207 — Camera (ortho for UI, perspective for world — perspective is
  stubbed in this phase).
- 208 — This capstone demo.

## Acceptance

- `./run-phase-demo.sh` → choose 2 → window opens, BLP renders, HUD
  updates each frame.
- Closes cleanly on `ESC` or window close; ASan-clean.

## Related

- Blocked by: all of Phase 1.
