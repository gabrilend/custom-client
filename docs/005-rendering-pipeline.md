# 005 — Rendering Pipeline

raylib (OpenGL 3.3 core under the hood). One window, one main thread, no
raylib or `rlgl` calls from anywhere else. The high-level raylib API
handles the per-frame `BeginDrawing` / `EndDrawing` cycle for us; we drop
down to `rlgl` for terrain splatting, M2 skinning, and other custom
shader work. Frame structure below.

## Frame outline (main thread)

```
poll input ──▶ pump network queue ──▶ simulation tick ──▶
   build scene ──▶ run Lua UI scripts ──▶ build UI draw list ──▶
      render passes ──▶ swap
```

Simulation tick is a fixed 30 Hz (matching the original world-server tick).
Rendering decouples: it interpolates entity positions between the last two
ticks based on accumulated wall-clock time.

## Render passes (in order)

1. **Depth pre-pass for opaques** — terrain + WMO opaques. Cheap fill, kills
   overdraw in the lit pass.
2. **Shadow map** — directional, one cascade for v1. Two cascades is a
   straightforward extension once the cascade math is parameterized.
3. **Opaque pass** — terrain (texture splatting from ADT alpha maps), WMO
   opaques, M2 opaques.
4. **Sky + atmosphere** — skybox, sun disk, distant fog applied as
   exponential height fog in the shader.
5. **Transparent pass** — water (planar reflection at quality > medium),
   M2 transparents sorted back-to-front, particles, ribbons.
6. **Full-screen overlays** — vignette, weather effects (rain, snow),
   underwater tint, "drowsy" / `daze` overlays driven by gameplay state.
7. **UI pass** — single ortho projection, draws the FrameXML widget tree
   that Lua built this frame.

## Asset pipeline (streamer thread → main thread)

1. Asset request enqueued by gameplay or the renderer.
2. Streamer reads from MPQ (StormLib), decompresses, parses to an
   intermediate CPU-side struct.
3. For GPU resources: streamer stages bytes into a thread-owned scratch
   buffer and enqueues the upload request.
4. Main thread, at the top of the next frame: walks the upload ring,
   issues the actual `rlLoadTexture` / `rlUpdateTexture` /
   `rlLoadVertexBuffer` calls, and returns the handle to the requesting
   subsystem.

This means assets never block the main thread on I/O or decompression, and
all raylib/rlgl calls stay single-threaded.

## Shaders

Hand-written GLSL 330. We do **not** use a shader cross-compiler. One shader
per material kind (terrain, M2-opaque, M2-skinned-transparent, water,
particle). UI uses raylib's default shaders. Permutations via `#define` and
a small cache. Loaded via `LoadShader` for raylib-managed materials,
`rlLoadShaderCode` for the lower-level cases.

## Camera

Right-handed, +Z up (matches the original asset coordinate system). Default
projection: 70° vertical FOV, 0.1 → 2000.0 near/far. Player camera is
spring-arm + chase, configured from gameplay state.

## What we deliberately skip in v1

- Deferred shading.
- HDR / tonemapping. We can fake it with a post-process LUT later.
- Vulkan / D3D backends.
- Mesh shaders, indirect draw, bindless textures.

## Related

- [003-asset-formats.md](./003-asset-formats.md) — what feeds the pipeline.
- [001-architecture-overview.md](./001-architecture-overview.md) — threading
  model.
