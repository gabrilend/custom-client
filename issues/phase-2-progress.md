# Phase 2 — Rendering Core — Progress

The goal of Phase 2 is to bridge Phase 1's headless asset reading to an
on-screen pixel. By the end, BLPs streamed from the live MPQ chain are
visible on screen through a project-owned sprite renderer + 2D camera +
custom shaders, with the game's actual fonts loaded and a hot-reload
loop for shader iteration.

## Issues

| ID  | Title                                            | Status   | Blocked by    |
|-----|--------------------------------------------------|----------|---------------|
| 201 | [Window and context](./201-window-and-context.md) | pending  | 102, 103      |
| 202 | [Math primitives](./202-math-primitives.md)      | pending  | 101           |
| 203 | [Shader pipeline](./203-shader-pipeline.md)      | pending  | 201, 202      |
| 204 | [BLP → GPU upload](./204-blp-to-gpu-upload.md)   | pending  | 107, 201      |
| 205 | [Sprite renderer](./205-sprite-renderer.md)      | pending  | 202, 203, 204 |
| 206 | [Bitmap text](./206-bitmap-text.md)              | pending  | 105, 203      |
| 207 | [Camera](./207-camera.md)                        | pending  | 201, 202      |
| 208 | [Phase 2 demo](./208-phase-demo.md)              | pending  | 201–207       |

## Suggested order

201 → 202 → 203 ∥ 204 → 205 ∥ 206 ∥ 207 → 208

(203 / 204 can land in parallel once 201+202 are in; 205, 206, 207 can
land in parallel once their respective blockers are in.)

## Phase-end deliverable

A windowed `phase-2-demo` reachable via `./run-phase-demo.sh` → `2`,
showing a navigable grid of real Blizzard BLPs with FRIZQT-rendered
labels and an HUD reporting renderer stats.

## Updates

_(append entries here as issues complete, newest at the bottom)_
