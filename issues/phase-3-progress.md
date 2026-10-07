# Phase 3 — World Geometry — Progress

The goal of Phase 3 is to get a recognizable corner of Azeroth on
screen: parsed ADTs streamed through a dedicated thread, terrain
rendered with the splat shader, WMOs rendered with portal-aware
visibility, Light.dbc-driven lighting + a skybox, and a free-fly
camera with proper frustum culling.

## Issues

| ID  | Title                                                       | Status  | Blocked by              |
|-----|-------------------------------------------------------------|---------|-------------------------|
| 301 | [ADT parser](./301-adt-parser.md)                           | partly built | 105                     |
| 302 | [Terrain renderer](./302-terrain-renderer.md)               | partly built | 301, 203, 204, 207      |
| 303 | [WDT + streaming](./303-wdt-and-streaming.md)               | partly built | 301, 302                |
| 304 | [WMO parser](./304-wmo-parser.md)                           | partly built | 105, 301 (shared iter)  |
| 305 | [WMO renderer + portals](./305-wmo-renderer.md)             | partly built | 304, 203, 204, 207      |
| 306 | [Light.dbc + sky](./306-light-and-sky.md)                   | partly built | 106, 302, 305, 207      |
| 307 | [Free-fly camera + frustum culling](./307-freefly-and-culling.md) | partly built | 207                     |
| 308 | [Phase 3 demo](./308-phase-demo.md)                         | pending | 301–307                 |
| 309 | [Water](./309-water.md)                                     | partly built | 301, 306          |
| 310 | [Shadows and live light](./310-shadows-and-live-light.md)   | pending (baked and blob built) | 302, 305, 306 |

## Suggested order

```
                       ┌─► 302 ─┐
301 ─┬─► (shared iter) │        ├─► 306 ─┐
     └──────────────────┘        │       │
                       ┌─► 304 ──┴► 305 ─┤
                       │                 ├─► 308
                       │  307 ───────────┘
                       │
                       └─► 303 (uses 302)
```

301 unlocks both 302 and 304 (they share the chunked-format iterator).
303 lands once 302 is ready to take uploads. 305 lands once 304 +
203/204/207 are in. 306 lights everything. 307 can land any time after
207. 308 composes them all.

## Phase-end deliverable

`./run-phase-demo.sh` → `3` opens a window at Northshire Abbey, with
real ADT terrain, recognizable WMOs (chapel, abbey), Elwynn-tinted
lighting, a free-fly camera, and a stats HUD reporting tile-stream and
culling rates.

## Updates

_(append entries here as issues complete, newest at the bottom)_

2026-10-07: the code from wow-chat-2026's custom client arrived (its issue
173v). "Partly built" marks an issue whose Current behavior now describes
built code; each says what is left. Counts: `lua /home/ritz/programming/ai-stuff/scripts/progress-dashboard.lua /mnt/mtwo/games/azeroth-core/custom-client -m`.
