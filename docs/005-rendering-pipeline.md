# 005 — Rendering Pipeline

raylib on OpenGL 3.3 core. One window; every OpenGL call on the main
thread. Rewritten 2026-10-07 for the merge: what is below **built** came
from wow-chat-2026's custom client (issues 173f to 173u), measured at
Astranaar; what is **planned** is this project's earlier plan, kept.

The step-by-step flow of data (files → memory → card → screen) is the
datapath document that moves here with the code (wow-chat-2026's
`docs/custom-client-datapath.md`); the measured history of making the
frame fast is its optimization checklist
(`docs/custom-client-optimization-checklist.md`), moving too.

## The rule

The processor decides *what* and *where*: a few numbers per object (a
place, a bone table, a colour). The graphics card does the per-point and
per-pixel work. Fewer, larger draw orders beat many small ones. Measured:
a frame at Astranaar with its 467 creatures went from 36 ms to about 13
by following it (2026-10-06).

## The frame (main thread), as built

```
tiles in and out ──▶ input and movement ──▶ camera ──▶ animation ──▶
   light for here and now ──▶ ground ──▶ doodads and buildings ──▶
      figures and creatures ──▶ water ──▶ particles ──▶ sky ──▶ swap
```

1. **Tiles.** The tiles round her are loaded and those left behind
   dropped. A second thread reads the next ring of tiles' files ahead and
   prepares their ground (points, light, blend pictures, unpacked
   pictures with their smaller copies); the main thread only sends what
   is ready to the card. Creatures are built as they come within 110
   yards, a few each frame.
2. **Input and movement.** The mover answers from the server's own
   heights, the models' solid shapes and the water, so she walks where
   the server lets her.
3. **Animation.** Bones are posed on the processor; the card bends the
   points.
4. **Light.** The zone's light tables give the sun, shade, sky colours,
   fog and water colours for her place and hour; the underwater set
   when the camera is below the surface.

## How each kind of thing is drawn (built)

| Thing | How |
|-------|-----|
| Ground | each tile in four meshes; every ground picture is a layer of one shared texture array; each patch's four layer numbers ride on its points; the tile's blend maps (and baked shadow) are one 1024 × 1024 picture; quarters out of sight not drawn |
| Doodads and buildings | each model's copies in sight gathered each frame and drawn together; compressed pictures sent as stored (DXT); two-layer pieces, moving pictures and animated tints in the shader |
| Swaying and camera-facing doodads | bones and weights on the points, sent once; the bone poses each frame as a float picture (one row a model, or a row per copy for camera-facing ones) |
| Characters and creatures | bent on the card the same way; a character's skin painted once from its pieces |
| Water | per-layer meshes, sight-tested; the zone's water colours and opacities |
| Particles | run on the processor near the camera; drawn as squares facing the camera |
| Sky | the dome sent once, its five colours as shader values; skybox models (Northrend's auroras) drawn round the camera; the far horizon from the map's low-detail heights |

Every picture goes through one filtering setting (bilinear, trilinear,
anisotropic up to 16x).

## Planned (kept from the earlier plan)

- **Shadow map** (one directional cascade), and torch and spell light
  reaching the world: in the merged engine (173v, answer 11).
- **Full-screen overlays:** weather, the ghost world, the drunk blur.
- **The interface pass:** one orthographic projection drawing the
  interface tree Lua built this frame.
- **Particles in one batch** and doodad pieces merged: measured small
  for now (wow-chat-2026 173u, items 2b and 6).
- **Pictures without a window:** a scene drawn straight into a picture
  file (the test scenes, wow-chat-2026 176a).
- **WC3 models, animations and particles** drawn by the same renderer
  ([001](./001-architecture-overview.md), "one engine, two games").

## Shaders

Hand-written GLSL 330, one per kind of thing (ground, models, water,
sky, particles); no cross-compiler.

## Camera

+Z up, the game's own axes (x north, y west). 60° vertical field of
view, near 0.3 yards, far a little past the view distance (or the far
horizon). Third person behind her, with the original's following styles,
and first person; the WC3 top-down camera is Phase 11.

## Not in v1

Deferred shading, HDR and tone mapping, Vulkan or Direct3D, mesh
shaders, bindless textures.

## Related

- [003-asset-formats.md](./003-asset-formats.md): what feeds the frame.
- [001-architecture-overview.md](./001-architecture-overview.md): threads.
